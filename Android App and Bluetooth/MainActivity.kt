package com.example.motorcontrollerapp // REMEMBER TO KEEP YOUR PACKAGE NAME!

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
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.util.*

@SuppressLint("MissingPermission")
class MainActivity : AppCompatActivity() {

    // UI Elements
    private lateinit var tvConnectionStatus: TextView
    private lateinit var btnConnect: Button
    private lateinit var btnStartStop: Button
    private lateinit var swReverse: Switch
    private lateinit var rgMode: RadioGroup

    // New RPM UI Elements
    private lateinit var btnMinusRpm: Button
    private lateinit var btnPlusRpm: Button
    private lateinit var btnSetRpm: Button
    private lateinit var etTargetRpm: EditText
    private lateinit var tvIntendedRpm: TextView
    private lateinit var tvMeasuredRpm: TextView

    // State Variables
    private var isMotorRunning = false
    private var currentRpm = 0
    private var currentMode = "MANUAL" // "MANUAL", "MAINTAIN", "SYNCED"

    // Bluetooth Variables
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var bluetoothSocket: BluetoothSocket? = null
    private var outputStream: OutputStream? = null
    private var inputStream: InputStream? = null
    private val SPP_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

    // Scanning Variables
    private val discoveredDevices = mutableListOf<BluetoothDevice>()
    private lateinit var deviceListAdapter: ArrayAdapter<String>

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Initialize UI
        tvConnectionStatus = findViewById(R.id.tvConnectionStatus)
        btnConnect = findViewById(R.id.btnConnect)
        btnStartStop = findViewById(R.id.btnStartStop)
        swReverse = findViewById(R.id.swReverse)
        rgMode = findViewById(R.id.rgMode)

        btnMinusRpm = findViewById(R.id.btnMinusRpm)
        btnPlusRpm = findViewById(R.id.btnPlusRpm)
        btnSetRpm = findViewById(R.id.btnSetRpm)
        etTargetRpm = findViewById(R.id.etTargetRpm)
        tvIntendedRpm = findViewById(R.id.tvIntendedRpm)
        tvMeasuredRpm = findViewById(R.id.tvMeasuredRpm)

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        requestBluetoothPermissions()
        val filter = IntentFilter(BluetoothDevice.ACTION_FOUND)
        registerReceiver(receiver, filter)
        deviceListAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1)

        // Listeners
        btnConnect.setOnClickListener { showDeviceSelectionDialog() }

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

        swReverse.setOnCheckedChangeListener { _, isChecked ->
            if (isChecked) sendCommand("DIR:REVERSE") else sendCommand("DIR:FORWARD")
        }

        // Mode Selection Logic
        rgMode.setOnCheckedChangeListener { _, checkedId ->
            when (checkedId) {
                R.id.rbManual -> {
                    currentMode = "MANUAL"
                    setRpmControlsEnabled(true)
                    sendCommand("MODE:MANUAL")
                    sendCommand("RPM:$currentRpm")
                }
                R.id.rbMaintain -> {
                    currentMode = "MAINTAIN"
                    setRpmControlsEnabled(false)
                    sendCommand("MODE:MAINTAIN")
                }
                R.id.rbSynced -> {
                    currentMode = "SYNCED"
                    setRpmControlsEnabled(false)
                    sendCommand("MODE:SYNCED")
                }
            }
        }

        // RPM Control Logic
        btnMinusRpm.setOnClickListener {
            currentRpm -= 25
            if (currentRpm < 0) currentRpm = 0
            updateRpmDisplayAndSend()
        }

        btnPlusRpm.setOnClickListener {
            currentRpm += 25
            updateRpmDisplayAndSend()
        }

        btnSetRpm.setOnClickListener {
            val inputText = etTargetRpm.text.toString()
            if (inputText.isNotEmpty()) {
                currentRpm = inputText.toInt()
                updateRpmDisplayAndSend()
            }
        }
    }

    private fun setRpmControlsEnabled(isEnabled: Boolean) {
        btnMinusRpm.isEnabled = isEnabled
        btnPlusRpm.isEnabled = isEnabled
        btnSetRpm.isEnabled = isEnabled
        etTargetRpm.isEnabled = isEnabled
    }

    private fun updateRpmDisplayAndSend() {
        etTargetRpm.setText(currentRpm.toString())
        tvIntendedRpm.text = "Intended RPM: $currentRpm"
        if (currentMode == "MANUAL") {
            sendCommand("RPM:$currentRpm")
        }
    }

    // --- BLUETOOTH LOGIC ---

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

    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            if (BluetoothDevice.ACTION_FOUND == intent.action) {
                val device: BluetoothDevice? = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
                if (device?.name != null && !discoveredDevices.contains(device)) {
                    discoveredDevices.add(device)
                    deviceListAdapter.add("${device.name}\n${device.address}")
                    deviceListAdapter.notifyDataSetChanged()
                }
            }
        }
    }

    private fun showDeviceSelectionDialog() {
        if (bluetoothAdapter == null) return
        discoveredDevices.clear()
        deviceListAdapter.clear()

        bluetoothAdapter?.bondedDevices?.forEach { device ->
            discoveredDevices.add(device)
            deviceListAdapter.add("${device.name} (Paired)\n${device.address}")
        }

        bluetoothAdapter?.startDiscovery()
        Toast.makeText(this, "Scanning...", Toast.LENGTH_SHORT).show()

        AlertDialog.Builder(this)
            .setTitle("Select Raspberry Pi")
            .setAdapter(deviceListAdapter) { _, which ->
                bluetoothAdapter?.cancelDiscovery()
                handleDeviceSelection(discoveredDevices[which])
            }
            .setNegativeButton("Cancel") { dialog, _ ->
                bluetoothAdapter?.cancelDiscovery()
                dialog.dismiss()
            }
            .show()
    }

    private fun handleDeviceSelection(device: BluetoothDevice) {
        if (device.bondState != BluetoothDevice.BOND_BONDED) {
            Toast.makeText(this, "Pairing... Click Connect again once paired.", Toast.LENGTH_LONG).show()
            device.createBond()
            return
        }
        connectToDevice(device)
    }

    private fun connectToDevice(device: BluetoothDevice) {
        tvConnectionStatus.text = "Status: Connecting..."

        Thread {
            try {
                bluetoothSocket = device.createRfcommSocketToServiceRecord(SPP_UUID)
                bluetoothSocket?.connect()
                outputStream = bluetoothSocket?.outputStream
                inputStream = bluetoothSocket?.inputStream

                runOnUiThread {
                    tvConnectionStatus.text = "Status: Connected to ${device.name}"
                    Toast.makeText(this, "Connected!", Toast.LENGTH_SHORT).show()
                }

                // Start listening for incoming telemetry data
                listenForData()

            } catch (e: IOException) {
                runOnUiThread {
                    tvConnectionStatus.text = "Status: Connection Failed"
                }
                try { bluetoothSocket?.close() } catch (e: IOException) { }
            }
        }.start()
    }

    private fun sendCommand(command: String) {
        val formattedCommand = "$command\n"
        try {
            outputStream?.write(formattedCommand.toByteArray())
        } catch (e: Exception) {
            // Connection lost
        }
    }

    // Listens for incoming data from the Raspberry Pi (e.g., "MEASURED_RPM:1500")
    private fun listenForData() {
        val buffer = ByteArray(1024)
        var bytes: Int

        while (true) {
            try {
                bytes = inputStream?.read(buffer) ?: -1
                if (bytes > 0) {
                    val incomingMessage = String(buffer, 0, bytes).trim()

                    // If the Pi sends back data starting with "MEASURED_RPM:"
                    if (incomingMessage.startsWith("MEASURED_RPM:")) {
                        val rpmValue = incomingMessage.substringAfter(":")

                        // Always update UI on the main thread
                        runOnUiThread {
                            tvMeasuredRpm.text = "Measured RPM: $rpmValue"
                        }
                    }
                }
            } catch (e: IOException) {
                runOnUiThread {
                    tvConnectionStatus.text = "Status: Disconnected"
                }
                break // Exit the loop if connection drops
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(receiver)
        try { bluetoothSocket?.close() } catch (e: IOException) { }
    }
}