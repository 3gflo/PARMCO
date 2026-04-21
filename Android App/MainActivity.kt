package com.example.motorcontrollerapp // Ensure this matches your package name!

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

// Suppresses warnings for missing Bluetooth permissions in the IDE. 
// We are handling these permissions manually in requestBluetoothPermissions().
@SuppressLint("MissingPermission")
class MainActivity : AppCompatActivity() {

    // --- UI Elements ---
    // 'lateinit' means we promise to initialize these variables later (in onCreate) 
    // before we ever try to use them.
    private lateinit var tvConnectionStatus: TextView
    private lateinit var btnConnect: Button
    private lateinit var btnStartStop: Button
    private lateinit var swReverse: Switch
    private lateinit var rgMode: RadioGroup

    // Maintain Mode Elements
    private lateinit var etTargetRpm: EditText
    private lateinit var btnSetTargetRpm: Button

    // Manual Output UI Elements
    private lateinit var btnMinusRpm: Button
    private lateinit var btnPlusRpm: Button
    private lateinit var tvIntendedPwm: TextView
    private lateinit var tvMeasuredRpm: TextView
    private lateinit var tvExternalRpm: TextView // External Telemetry UI

    // --- State Variables ---
    // These track the current logical state of the app so it knows what to send to the Pi.
    private var isMotorRunning = false
    private var currentPwm = 0
    private var currentMode = "MANUAL"

    // --- Bluetooth Variables ---
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var bluetoothSocket: BluetoothSocket? = null
    private var outputStream: OutputStream? = null
    private var inputStream: InputStream? = null
    
    // The SPP (Serial Port Profile) UUID. This is the standard, universally agreed-upon 
    // ID used for generic Bluetooth serial communication (like talking to a Raspberry Pi or Arduino).
    private val SPP_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

    // --- Scanning Variables ---
    // Lists to hold the devices we find during a Bluetooth scan, and the adapter to show them in a UI list.
    private val discoveredDevices = mutableListOf<BluetoothDevice>()
    private lateinit var deviceListAdapter: ArrayAdapter<String>

    // onCreate is the entry point for an Android Activity. It runs when the app screen is first created.
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // Links this Kotlin code to your activity_main.xml layout file.
        setContentView(R.layout.activity_main)

        // --- Initialize UI ---
        // Binding the Kotlin variables to the actual UI components in the XML using their IDs.
        tvConnectionStatus = findViewById(R.id.tvConnectionStatus)
        btnConnect = findViewById(R.id.btnConnect)
        btnStartStop = findViewById(R.id.btnStartStop)
        swReverse = findViewById(R.id.swReverse)
        rgMode = findViewById(R.id.rgMode)

        etTargetRpm = findViewById(R.id.etTargetRpm)
        btnSetTargetRpm = findViewById(R.id.btnSetTargetRpm)

        btnMinusRpm = findViewById(R.id.btnMinusRpm)
        btnPlusRpm = findViewById(R.id.btnPlusRpm)
        tvIntendedPwm = findViewById(R.id.tvIntendedPwm)
        tvMeasuredRpm = findViewById(R.id.tvMeasuredRpm)
        tvExternalRpm = findViewById(R.id.tvExternalRpm) 

        // Get the system's Bluetooth service to control the phone's Bluetooth radio.
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        // Ask the user for Bluetooth permissions right away.
        requestBluetoothPermissions()
        
        // Register a listener (receiver) that the Android OS will ping every time it finds a new Bluetooth device.
        val filter = IntentFilter(BluetoothDevice.ACTION_FOUND)
        registerReceiver(receiver, filter)
        
        // Setup the adapter that will format our discovered devices into a simple text list for the dialog pop-up.
        deviceListAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1)

        // --- Initial UI State ---
        // Force the initial UI to match the backend variables before anything happens.
        rgMode.check(R.id.rbManual)
        setManualControlsEnabled(true)
        setMaintainControlsEnabled(false)

        // --- Listeners ---
        // These blocks define what happens when users interact with the UI.

        btnConnect.setOnClickListener { showDeviceSelectionDialog() }

        btnStartStop.setOnClickListener {
            isMotorRunning = !isMotorRunning // Toggle the boolean state
            if (isMotorRunning) {
                btnStartStop.text = "Stop Motor"

                // Auto-send target on start if in Maintain mode
                // This ensures that if the user typed an RPM but forgot to click the "Set" button, 
                // the Pi still gets the target value before the motor spins up.
                if (currentMode == "MAINTAIN") {
                    val targetString = etTargetRpm.text.toString()
                    if (targetString.isNotEmpty()) {
                        sendCommand("TARGET_RPM:${targetString.toInt()}")
                    }
                }

                sendCommand("STATE:START")
            } else {
                btnStartStop.text = "Start Motor"
                sendCommand("STATE:STOP")
            }
        }

        swReverse.setOnCheckedChangeListener { _, isChecked ->
            if (isChecked) sendCommand("DIR:REVERSE") else sendCommand("DIR:FORWARD")
        }

        // Mode Selection Logic (Radio Buttons)
        // Disables/enables the relevant UI components based on the selected mode.
        rgMode.setOnCheckedChangeListener { _, checkedId ->
            when (checkedId) {
                R.id.rbManual -> {
                    currentMode = "MANUAL"
                    setManualControlsEnabled(true)
                    setMaintainControlsEnabled(false)
                    sendCommand("MODE:MANUAL")
                    sendCommand("PWM:$currentPwm") // Instantly apply current PWM slider value
                }
                R.id.rbMaintain -> {
                    currentMode = "MAINTAIN"
                    setManualControlsEnabled(false)
                    setMaintainControlsEnabled(true)
                    sendCommand("MODE:MAINTAIN")
                }
                R.id.rbSynced -> {
                    currentMode = "SYNCED"
                    setManualControlsEnabled(false)
                    setMaintainControlsEnabled(false)
                    sendCommand("MODE:SYNCED") // Pi will see this and engage external loop
                }
            }
        }

        // Target RPM Submit Button (For Maintain Mode)
        btnSetTargetRpm.setOnClickListener {
            val targetString = etTargetRpm.text.toString()
            if (targetString.isNotEmpty()) {
                val targetRpm = targetString.toInt()
                sendCommand("TARGET_RPM:$targetRpm")
                Toast.makeText(this, "Target RPM set to $targetRpm", Toast.LENGTH_SHORT).show()
            }
        }

        // Manual PWM Control Logic (+ / - Buttons)
        // Adjusts the PWM variable, clamps it between 0 and 1000, then sends it.
        btnMinusRpm.setOnClickListener {
            currentPwm -= 50
            if (currentPwm < 0) currentPwm = 0
            updatePwmDisplayAndSend()
        }

        btnPlusRpm.setOnClickListener {
            currentPwm += 50
            if (currentPwm > 1000) currentPwm = 1000
            updatePwmDisplayAndSend()
        }
    }

    // --- Helper Functions ---

    private fun setManualControlsEnabled(isEnabled: Boolean) {
        btnMinusRpm.isEnabled = isEnabled
        btnPlusRpm.isEnabled = isEnabled
    }

    private fun setMaintainControlsEnabled(isEnabled: Boolean) {
        etTargetRpm.isEnabled = isEnabled
        btnSetTargetRpm.isEnabled = isEnabled
    }

    private fun updatePwmDisplayAndSend() {
        tvIntendedPwm.text = "PWM: $currentPwm"
        if (currentMode == "MANUAL") {
            sendCommand("PWM:$currentPwm")
        }
    }

    // --- BLUETOOTH LOGIC ---

    // Handles the complex permission rules introduced in recent Android versions.
    private fun requestBluetoothPermissions() {
        val permissions = mutableListOf<String>()
        // Android 12 (API 31) and higher require specific BLUETOOTH_CONNECT and SCAN permissions.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
            permissions.add(Manifest.permission.BLUETOOTH_SCAN)
        } else {
            // Older Android versions bundled Bluetooth scanning inside location permissions.
            permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }
        ActivityCompat.requestPermissions(this, permissions.toTypedArray(), 1)
    }

    // A BroadcastReceiver listens for system-wide events. This one listens for ACTION_FOUND,
    // which the OS fires every time the phone's antenna detects a nearby Bluetooth device.
    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            if (BluetoothDevice.ACTION_FOUND == intent.action) {
                // Extract the device data from the broadcast intent
                val device: BluetoothDevice? = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
                // If it has a name and we haven't seen it yet, add it to our UI list.
                if (device?.name != null && !discoveredDevices.contains(device)) {
                    discoveredDevices.add(device)
                    deviceListAdapter.add("${device.name}\n${device.address}")
                    deviceListAdapter.notifyDataSetChanged() // Refreshes the dialog UI
                }
            }
        }
    }

    // Pops up a dialog box allowing the user to select their Pi.
    private fun showDeviceSelectionDialog() {
        if (bluetoothAdapter == null) return
        
        // Clear previous scan results to start fresh
        discoveredDevices.clear()
        deviceListAdapter.clear()

        // First, add devices the phone has already paired with in the past (like the Pi).
        bluetoothAdapter?.bondedDevices?.forEach { device ->
            discoveredDevices.add(device)
            deviceListAdapter.add("${device.name} (Paired)\n${device.address}")
        }

        // Then, start scanning for new, unpaired devices in the area.
        bluetoothAdapter?.startDiscovery()
        Toast.makeText(this, "Scanning...", Toast.LENGTH_SHORT).show()

        // Build and display the pop-up window
        AlertDialog.Builder(this)
            .setTitle("Select Raspberry Pi")
            .setAdapter(deviceListAdapter) { _, which ->
                // Stop scanning once the user makes a selection (scanning drains battery heavily)
                bluetoothAdapter?.cancelDiscovery()
                handleDeviceSelection(discoveredDevices[which])
            }
            .setNegativeButton("Cancel") { dialog, _ ->
                bluetoothAdapter?.cancelDiscovery()
                dialog.dismiss()
            }
            .show()
    }

    // Checks if the device is paired before trying to connect.
    private fun handleDeviceSelection(device: BluetoothDevice) {
        if (device.bondState != BluetoothDevice.BOND_BONDED) {
            Toast.makeText(this, "Pairing... Click Connect again once paired.", Toast.LENGTH_LONG).show()
            device.createBond() // Prompts the OS pairing dialog (PIN code entry, etc.)
            return
        }
        connectToDevice(device)
    }

    // Establishes the actual socket connection.
    private fun connectToDevice(device: BluetoothDevice) {
        tvConnectionStatus.text = "Status: Connecting..."

        // VERY IMPORTANT: Network/Bluetooth connections block the thread they run on. 
        // If we ran this on the main UI thread, the app would completely freeze until it connected.
        // Therefore, we open a background Thread.
        Thread {
            try {
                // Create the secure connection tube (socket) to the Pi.
                bluetoothSocket = device.createRfcommSocketToServiceRecord(SPP_UUID)
                bluetoothSocket?.connect()
                
                // Grab the input and output streams. These are how we push and pull data bytes.
                outputStream = bluetoothSocket?.outputStream
                inputStream = bluetoothSocket?.inputStream

                // We are in a background thread, but UI elements can ONLY be updated from the main UI thread.
                // runOnUiThread safely jumps back to the main thread to update the text.
                runOnUiThread {
                    tvConnectionStatus.text = "Status: Connected to ${device.name}"
                    Toast.makeText(this, "Connected! Syncing states...", Toast.LENGTH_SHORT).show()
                }

                // --- Sync State on Connection ---
                // Tell the Pi exactly what the App UI is currently set to, so they are perfectly mirrored.
                sendCommand("MODE:$currentMode")

                if (currentMode == "MANUAL") {
                    sendCommand("PWM:$currentPwm")
                } else if (currentMode == "MAINTAIN") {
                    val targetString = etTargetRpm.text.toString()
                    if (targetString.isNotEmpty()) {
                        sendCommand("TARGET_RPM:${targetString.toInt()}")
                    }
                }

                if (swReverse.isChecked) sendCommand("DIR:REVERSE") else sendCommand("DIR:FORWARD")

                // Start an infinite loop to listen for incoming telemetry data.
                listenForData()

            } catch (e: IOException) {
                // If the connection fails (e.g., Pi is off, out of range), catch the error gracefully.
                runOnUiThread {
                    tvConnectionStatus.text = "Status: Connection Failed"
                }
                try { bluetoothSocket?.close() } catch (e: IOException) { }
            }
        }.start() // Start the thread
    }

    // Converts our String commands into a ByteArray and shoves them through the Bluetooth tube.
    private fun sendCommand(command: String) {
        val formattedCommand = "$command\n" // The Pi expects a newline character to know the command is finished
        try {
            outputStream?.write(formattedCommand.toByteArray())
        } catch (e: Exception) {
            // Do nothing if it fails; the listenForData thread will handle the disconnection UI.
        }
    }

    // This loop runs infinitely in the background while connected, waiting for the Pi to say something.
    private fun listenForData() {
        val buffer = ByteArray(1024)
        var bytes: Int

        while (true) {
            try {
                // read() pauses the thread here until bytes actually arrive from the Pi.
                bytes = inputStream?.read(buffer) ?: -1
                if (bytes > 0) {
                    // Convert the raw bytes into a readable String.
                    val incomingData = String(buffer, 0, bytes)

                    // Split the incoming stream by newline (\n or \r). 
                    // This is crucial because if the Pi sends "MEASURED_RPM:500\nEXTERNAL_RPM:400" 
                    // fast enough, they arrive in a single batch of bytes.
                    val lines = incomingData.split("\n", "\r")

                    for (line in lines) {
                        val cleanLine = line.trim() // Removes extra invisible whitespace

                        // Check the prefix of the string to figure out what data the Pi sent.
                        if (cleanLine.startsWith("MEASURED_RPM:")) {
                            val rpmValue = cleanLine.substringAfter(":") // Extracts just the number part
                            runOnUiThread {
                                tvMeasuredRpm.text = "Measured RPM: $rpmValue" // Update UI safely
                            }
                        }
                        else if (cleanLine.startsWith("EXTERNAL_RPM:")) {
                            val externalRpmValue = cleanLine.substringAfter(":")
                            runOnUiThread {
                                tvExternalRpm.text = "External RPM: $externalRpmValue"
                            }
                        }
                    }
                }
            } catch (e: IOException) {
                // An IOException here usually means the Bluetooth connection was physically lost or closed.
                runOnUiThread {
                    tvConnectionStatus.text = "Status: Disconnected"
                }
                break // Break the infinite loop, killing the listening thread.
            }
        }
    }

    // onDestroy is called when the user fully closes the app.
    // It's important to clean up system resources (un-registering receivers, closing sockets) to prevent memory leaks.
    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(receiver)
        try { bluetoothSocket?.close() } catch (e: IOException) { }
    }
}