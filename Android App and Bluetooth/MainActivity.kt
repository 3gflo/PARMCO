package com.example.motorcontrollerapp // Ensure this matches your actual package name!

import android.Manifest
import android.annotation.SuppressLint
import android.app.AlertDialog
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.Bundle
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.util.*

@SuppressLint("MissingPermission") // Suppresses warnings since we handle permissions manually
class MainActivity : AppCompatActivity() {

    // --- UI ELEMENTS ---
    // These variables will hold references to the buttons and text views in the XML layout
    private lateinit var tvConnectionStatus: TextView
    private lateinit var btnConnect: Button
    private lateinit var btnStartStop: Button
    private lateinit var swReverse: Switch
    private lateinit var rgMode: RadioGroup
    private lateinit var btnMinusRpm: Button
    private lateinit var btnPlusRpm: Button
    private lateinit var tvIntendedPwm: TextView
    private lateinit var tvMeasuredRpm: TextView

    // --- STATE VARIABLES ---
    // Keep track of what the app is currently doing
    private var isMotorRunning = false
    private var currentRpm = 0 // Actually represents PWM value (0 to 1000)
    private var currentMode = "MANUAL"

    // --- BLUETOOTH VARIABLES ---
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var bluetoothSocket: BluetoothSocket? = null
    private var outputStream: OutputStream? = null // Used to send data TO the Pi
    private var inputStream: InputStream? = null   // Used to read data FROM the Pi

    // This UUID is a standard identifier for Serial Port Profile (SPP) Bluetooth connections
    private val SPP_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

    // --- SCANNING VARIABLES ---
    private val discoveredDevices = mutableListOf<BluetoothDevice>()
    private lateinit var deviceListAdapter: ArrayAdapter<String>

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main) // Links this code to the XML layout

        // 1. Link the code variables to the actual XML UI components using their IDs
        tvConnectionStatus = findViewById(R.id.tvConnectionStatus)
        btnConnect = findViewById(R.id.btnConnect)
        btnStartStop = findViewById(R.id.btnStartStop)
        swReverse = findViewById(R.id.swReverse)
        rgMode = findViewById(R.id.rgMode)
        btnMinusRpm = findViewById(R.id.btnMinusRpm)
        btnPlusRpm = findViewById(R.id.btnPlusRpm)
        tvIntendedPwm = findViewById(R.id.tvIntendedPwm)
        tvMeasuredRpm = findViewById(R.id.tvMeasuredRpm)

        // 2. Initialize the Bluetooth Adapter (the phone's physical Bluetooth hardware)
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        // Ask the user for Bluetooth permissions if they haven't granted them yet
        requestBluetoothPermissions()

        // Register a receiver to listen for Bluetooth devices being discovered nearby
        val filter = IntentFilter(BluetoothDevice.ACTION_FOUND)
        registerReceiver(receiver, filter)
        deviceListAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1)

        // ================= UI EVENT LISTENERS =================

        // When "Connect" is clicked, show a list of nearby/paired devices
        btnConnect.setOnClickListener { showDeviceSelectionDialog() }

        // When "Start/Stop" is clicked, toggle the state and send a command to the Pi
        btnStartStop.setOnClickListener {
            isMotorRunning = !isMotorRunning
            if (isMotorRunning) {
                btnStartStop.text = "Stop Motor"
                sendCommand("STATE:START")
            } else {
                btnStartStop.text = "Start Motor"
                sendCommand("STATE:STOP")
            }
        }

        // When Reverse switch is toggled, send the corresponding direction command
        swReverse.setOnCheckedChangeListener { _, isChecked ->
            if (isChecked) sendCommand("DIR:REVERSE") else sendCommand("DIR:FORWARD")
        }

        // When a new radio button is selected, change the operating mode
        rgMode.setOnCheckedChangeListener { _, checkedId ->
            when (checkedId) {
                R.id.rbManual -> {
                    currentMode = "MANUAL"
                    setRpmControlsEnabled(true) // Enable +/- buttons
                    sendCommand("MODE:MANUAL")
                    sendCommand("RPM:$currentRpm") // Send the current target immediately
                }
                R.id.rbMaintain -> {
                    currentMode = "MAINTAIN"
                    setRpmControlsEnabled(false) // Disable +/- buttons (Pi handles speed)
                    sendCommand("MODE:MAINTAIN")
                }
                R.id.rbSynced -> {
                    currentMode = "SYNCED"
                    setRpmControlsEnabled(false) // Disable +/- buttons (Sensor handles speed)
                    sendCommand("MODE:SYNCED")
                }
            }
        }

        // Decrease PWM Button: Subtracts 50, but never lets it go below 0
        btnMinusRpm.setOnClickListener {
            currentRpm -= 50
            if (currentRpm < 0) currentRpm = 0 // Floor cap
            updateRpmDisplayAndSend()
        }

        // Increase PWM Button: Adds 50, but never lets it go above 1000
        btnPlusRpm.setOnClickListener {
            currentRpm += 50
            if (currentRpm > 1000) currentRpm = 1000 // Ceiling cap at 1000 limit
            updateRpmDisplayAndSend()
        }
    }

    // Helper function to turn the +/- buttons grey/unclickable if we aren't in manual mode
    private fun setRpmControlsEnabled(isEnabled: Boolean) {
        btnMinusRpm.isEnabled = isEnabled
        btnPlusRpm.isEnabled = isEnabled
    }

    // Helper function to update the screen and send the new data to the Pi
    private fun updateRpmDisplayAndSend() {
        tvIntendedPwm.text = "PWM: $currentRpm"
        if (currentMode == "MANUAL") {
            sendCommand("RPM:$currentRpm")
        }
    }

    // ================= BLUETOOTH LOGIC =================

    // Handles the new Android 12+ Bluetooth permission requirements
    private fun requestBluetoothPermissions() {
        val permissions = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
            permissions.add(Manifest.permission.BLUETOOTH_SCAN)
        } else {
            permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }
        ActivityCompat.requestPermissions(this, permissions.toTypedArray(), 1)
    }

    // A background listener that catches new Bluetooth devices as the phone scans the room
    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            if (BluetoothDevice.ACTION_FOUND == intent.action) {
                val device: BluetoothDevice? = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
                if (device?.name != null && !discoveredDevices.contains(device)) {
                    discoveredDevices.add(device)
                    deviceListAdapter.add("${device.name}\n${device.address}")
                    deviceListAdapter.notifyDataSetChanged() // Refresh the list UI
                }
            }
        }
    }

    // Pops up a list of available devices to connect to
    private fun showDeviceSelectionDialog() {
        if (bluetoothAdapter == null) return
        discoveredDevices.clear()
        deviceListAdapter.clear()

        // First, add devices we've already paired with in the past
        bluetoothAdapter?.bondedDevices?.forEach { device ->
            discoveredDevices.add(device)
            deviceListAdapter.add("${device.name} (Paired)\n${device.address}")
        }

        // Then, start scanning for new devices
        bluetoothAdapter?.startDiscovery()
        Toast.makeText(this, "Scanning...", Toast.LENGTH_SHORT).show()

        // Show the list in an Android AlertDialog
        AlertDialog.Builder(this)
            .setTitle("Select Raspberry Pi")
            .setAdapter(deviceListAdapter) { _, which ->
                bluetoothAdapter?.cancelDiscovery() // Stop scanning once selected
                handleDeviceSelection(discoveredDevices[which])
            }
            .setNegativeButton("Cancel") { dialog, _ ->
                bluetoothAdapter?.cancelDiscovery()
                dialog.dismiss()
            }
            .show()
    }

    // Check if the device needs pairing before trying to connect
    private fun handleDeviceSelection(device: BluetoothDevice) {
        if (device.bondState != BluetoothDevice.BOND_BONDED) {
            Toast.makeText(this, "Pairing... Click Connect again once paired.", Toast.LENGTH_LONG).show()
            device.createBond()
            return
        }
        connectToDevice(device)
    }

    // Opens the actual communication channel (socket) to the Pi
    private fun connectToDevice(device: BluetoothDevice) {
        tvConnectionStatus.text = "Status: Connecting..."

        // We run this in a separate Thread so the UI doesn't freeze while trying to connect
        Thread {
            try {
                bluetoothSocket = device.createRfcommSocketToServiceRecord(SPP_UUID)
                bluetoothSocket?.connect()

                // Get the input/output streams so we can send/receive bytes
                outputStream = bluetoothSocket?.outputStream
                inputStream = bluetoothSocket?.inputStream

                // Update the UI on the main thread
                runOnUiThread {
                    tvConnectionStatus.text = "Status: Connected to ${device.name}"
                    Toast.makeText(this, "Connected!", Toast.LENGTH_SHORT).show()
                }

                // Start an infinite loop to listen for messages coming back from the Pi
                listenForData()

            } catch (e: IOException) {
                runOnUiThread {
                    tvConnectionStatus.text = "Status: Connection Failed"
                }
                try { bluetoothSocket?.close() } catch (e: IOException) { }
            }
        }.start()
    }

    // Formats a string (adds a newline character) and sends it over Bluetooth
    private fun sendCommand(command: String) {
        val formattedCommand = "$command\n"
        try {
            outputStream?.write(formattedCommand.toByteArray())
        } catch (e: Exception) {
            // Connection was lost while sending
        }
    }

    // An infinite loop that constantly reads incoming bytes from the Pi
    private fun listenForData() {
        val buffer = ByteArray(1024)
        var bytes: Int

        while (true) {
            try {
                // Read incoming bytes into the buffer array
                bytes = inputStream?.read(buffer) ?: -1
                if (bytes > 0) {
                    // Convert the raw bytes back into a readable string
                    val incomingMessage = String(buffer, 0, bytes).trim()

                    // If the Pi sends a message starting with MEASURED_RPM:
                    if (incomingMessage.startsWith("MEASURED_RPM:")) {
                        // Extract just the number part
                        val rpmValue = incomingMessage.substringAfter(":")

                        // Update the red text on the UI
                        runOnUiThread {
                            tvMeasuredRpm.text = "Measured RPM: $rpmValue"
                        }
                    }
                }
            } catch (e: IOException) {
                // If reading fails, the connection was lost
                runOnUiThread {
                    tvConnectionStatus.text = "Status: Disconnected"
                }
                break // Exit the infinite loop
            }
        }
    }

    // Clean up connections when the app is closed
    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(receiver)
        try { bluetoothSocket?.close() } catch (e: IOException) { }
    }
}