package com.example.safersignalapp

import android.Manifest
import android.annotation.SuppressLint
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.VibrationEffect
import android.os.Vibrator
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import java.util.UUID

class SaferSignalBleService : Service() {

    companion object {

        const val SERVICE_CHANNEL_ID = "safer_signal_monitor"
        const val ALARM_CHANNEL_ID = "safer_signal_alarm"

        const val SERVICE_NOTIFICATION_ID = 2001
        const val ALARM_NOTIFICATION_ID = 1001

        const val PREFS_NAME = "SaferSignalPrefs"
        const val SAVED_DEVICE_ADDRESS = "savedDeviceAddress"

        const val SAFER_SIGNAL_DEVICE_NAME = "Safer Signal"

        val SERVICE_UUID: UUID =
            UUID.fromString(
                "12345678-1234-1234-1234-123456789001"
            )

        val ALARM_UUID: UUID =
            UUID.fromString(
                "12345678-1234-1234-1234-123456789002"
            )

        val CCCD_UUID: UUID =
            UUID.fromString(
                "00002902-0000-1000-8000-00805f9b34fb"
            )

        const val ACTION_STATUS =
            "com.example.safersignalapp.STATUS"

        const val EXTRA_STATUS = "status"
        const val EXTRA_ALARM = "alarm"
    }

    private lateinit var bluetoothManager: BluetoothManager

    private val handler =
        Handler(Looper.getMainLooper())

    private var bluetoothGatt: BluetoothGatt? = null

    private var reconnectRunnable: Runnable? = null

    private var isScanning = false
    private var isConnecting = false
    private var isConnected = false

    private var alarmActive = false

    // ----------------------------------------------------
    // Bluetooth ON/OFF receiver
    // ----------------------------------------------------

    private val bluetoothStateReceiver =
        object : BroadcastReceiver() {

            override fun onReceive(
                context: Context?,
                intent: Intent?
            ) {

                if (
                    intent?.action ==
                    BluetoothAdapter.ACTION_STATE_CHANGED
                ) {

                    val state =
                        intent.getIntExtra(
                            BluetoothAdapter.EXTRA_STATE,
                            BluetoothAdapter.ERROR
                        )

                    when (state) {

                        BluetoothAdapter.STATE_OFF -> {

                            cancelReconnect()

                            stopBleScan()

                            isConnecting = false
                            isConnected = false

                            updateStatus(
                                "Bluetooth is turned off"
                            )
                        }

                        BluetoothAdapter.STATE_TURNING_OFF -> {

                            updateStatus(
                                "Bluetooth is turning off..."
                            )
                        }

                        BluetoothAdapter.STATE_TURNING_ON -> {

                            updateStatus(
                                "Bluetooth is turning on..."
                            )
                        }

                        BluetoothAdapter.STATE_ON -> {

                            updateStatus(
                                "Bluetooth on - reconnecting..."
                            )

                            /*
                             * Small delay gives the Android
                             * Bluetooth stack time to become ready.
                             */
                            handler.postDelayed(
                                {
                                    connectAutomatically()
                                },
                                1000
                            )
                        }
                    }
                }
            }
        }

    // ----------------------------------------------------
    // Service startup
    // ----------------------------------------------------

    override fun onCreate() {

        super.onCreate()

        bluetoothManager =
            getSystemService(
                BluetoothManager::class.java
            )

        createNotificationChannels()

        registerBluetoothReceiver()

        startForeground(
            SERVICE_NOTIFICATION_ID,
            buildMonitoringNotification(
                "Starting Safer Signal..."
            )
        )
    }

    @SuppressLint("MissingPermission")
    override fun onStartCommand(
        intent: Intent?,
        flags: Int,
        startId: Int
    ): Int {

        if (!hasBluetoothPermissions()) {

            updateStatus(
                "Bluetooth permission required"
            )

            return START_STICKY
        }

        /*
         * Prevent starting another connection
         * if we are already connected or connecting.
         */
        if (
            !isConnected &&
            !isConnecting
        ) {

            connectAutomatically()
        }

        return START_STICKY
    }

    override fun onBind(
        intent: Intent?
    ): IBinder? {

        return null
    }

    // ----------------------------------------------------
    // Register Bluetooth state receiver
    // ----------------------------------------------------

    private fun registerBluetoothReceiver() {

        val filter =
            IntentFilter(
                BluetoothAdapter.ACTION_STATE_CHANGED
            )

        if (
            Build.VERSION.SDK_INT >=
            Build.VERSION_CODES.TIRAMISU
        ) {

            registerReceiver(
                bluetoothStateReceiver,
                filter,
                Context.RECEIVER_EXPORTED
            )

        } else {

            @Suppress("DEPRECATION")
            registerReceiver(
                bluetoothStateReceiver,
                filter
            )
        }
    }

    // ----------------------------------------------------
    // Notification channels
    // ----------------------------------------------------

    private fun createNotificationChannels() {

        val manager =
            getSystemService(
                NotificationManager::class.java
            )

        val monitoringChannel =
            NotificationChannel(
                SERVICE_CHANNEL_ID,
                "Safer Signal Monitoring",
                NotificationManager.IMPORTANCE_LOW
            ).apply {

                description =
                    "Keeps Safer Signal connected and monitoring."
            }

        manager.createNotificationChannel(
            monitoringChannel
        )

        val alarmChannel =
            NotificationChannel(
                ALARM_CHANNEL_ID,
                "Safer Signal Emergency Alerts",
                NotificationManager.IMPORTANCE_HIGH
            ).apply {

                description =
                    "Emergency smoke alarm notifications"

                enableVibration(true)
            }

        manager.createNotificationChannel(
            alarmChannel
        )
    }

    private fun buildMonitoringNotification(
        message: String
    ) =
        NotificationCompat.Builder(
            this,
            SERVICE_CHANNEL_ID
        )
            .setSmallIcon(
                android.R.drawable.stat_sys_data_bluetooth
            )
            .setContentTitle(
                "Safer Signal"
            )
            .setContentText(
                message
            )
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setPriority(
                NotificationCompat.PRIORITY_LOW
            )
            .build()

    private fun updateMonitoringNotification(
        message: String
    ) {

        val manager =
            getSystemService(
                NotificationManager::class.java
            )

        manager.notify(
            SERVICE_NOTIFICATION_ID,
            buildMonitoringNotification(
                message
            )
        )
    }

    // ----------------------------------------------------
    // Permissions
    // ----------------------------------------------------

    private fun hasBluetoothPermissions(): Boolean {

        if (
            Build.VERSION.SDK_INT >=
            Build.VERSION_CODES.S
        ) {

            val scan =
                ContextCompat.checkSelfPermission(
                    this,
                    Manifest.permission.BLUETOOTH_SCAN
                )

            val connect =
                ContextCompat.checkSelfPermission(
                    this,
                    Manifest.permission.BLUETOOTH_CONNECT
                )

            if (
                scan != PackageManager.PERMISSION_GRANTED ||
                connect != PackageManager.PERMISSION_GRANTED
            ) {

                return false
            }
        }

        return true
    }

    // ----------------------------------------------------
    // Automatic connection
    // ----------------------------------------------------

    @SuppressLint("MissingPermission")
    private fun connectAutomatically() {

        if (!hasBluetoothPermissions()) {

            updateStatus(
                "Bluetooth permission required"
            )

            return
        }

        val adapter =
            bluetoothManager.adapter

        if (!adapter.isEnabled) {

            /*
             * Do NOT keep retrying here.
             * The Bluetooth state receiver will tell us
             * when Bluetooth is turned back on.
             */

            updateStatus(
                "Bluetooth is turned off"
            )

            return
        }

        if (
            isConnected ||
            isConnecting
        ) {

            return
        }

        val preferences =
            getSharedPreferences(
                PREFS_NAME,
                Context.MODE_PRIVATE
            )

        val savedAddress =
            preferences.getString(
                SAVED_DEVICE_ADDRESS,
                null
            )

        if (savedAddress != null) {

            try {

                val device =
                    adapter.getRemoteDevice(
                        savedAddress
                    )

                updateStatus(
                    "Reconnecting..."
                )

                connectToDevice(
                    device
                )

            } catch (e: Exception) {

                /*
                 * If saved address cannot be used,
                 * fall back to scanning.
                 */

                startScan()
            }

        } else {

            /*
             * First time setup.
             */

            startScan()
        }
    }

    // ----------------------------------------------------
    // BLE scan
    // ----------------------------------------------------

    @SuppressLint("MissingPermission")
    private fun startScan() {

        if (isScanning) {
            return
        }

        val adapter =
            bluetoothManager.adapter

        if (!adapter.isEnabled) {

            updateStatus(
                "Bluetooth is turned off"
            )

            return
        }

        val scanner =
            adapter.bluetoothLeScanner

        if (scanner == null) {

            updateStatus(
                "BLE scanner unavailable"
            )

            scheduleReconnect()

            return
        }

        updateStatus(
            "Searching for Safer Signal..."
        )

        isScanning = true

        scanner.startScan(
            scanCallback
        )
    }

    private val scanCallback =
        object : ScanCallback() {

            @SuppressLint("MissingPermission")
            override fun onScanResult(
                callbackType: Int,
                result: ScanResult
            ) {

                val device =
                    result.device

                val deviceName =
                    try {

                        device.name

                    } catch (
                        e: SecurityException
                    ) {

                        null
                    }

                if (
                    deviceName ==
                    SAFER_SIGNAL_DEVICE_NAME
                ) {

                    stopBleScan()

                    /*
                     * Save this specific ESP32.
                     * User only needs initial setup once.
                     */

                    getSharedPreferences(
                        PREFS_NAME,
                        Context.MODE_PRIVATE
                    )
                        .edit()
                        .putString(
                            SAVED_DEVICE_ADDRESS,
                            device.address
                        )
                        .apply()

                    updateStatus(
                        "Safer Signal found"
                    )

                    connectToDevice(
                        device
                    )
                }
            }

            override fun onScanFailed(
                errorCode: Int
            ) {

                isScanning = false

                updateStatus(
                    "Bluetooth scan failed"
                )

                scheduleReconnect()
            }
        }

    @SuppressLint("MissingPermission")
    private fun stopBleScan() {

        if (!isScanning) {
            return
        }

        try {

            bluetoothManager.adapter
                .bluetoothLeScanner
                ?.stopScan(
                    scanCallback
                )

        } catch (
            e: Exception
        ) {

            // Scanner may already be stopped.
        }

        isScanning = false
    }

    // ----------------------------------------------------
    // Connect to ESP32
    // ----------------------------------------------------

    @SuppressLint("MissingPermission")
    private fun connectToDevice(
        device: BluetoothDevice
    ) {

        if (
            isConnecting ||
            isConnected
        ) {

            return
        }

        stopBleScan()

        cancelReconnect()

        try {

            bluetoothGatt?.close()

        } catch (
            e: Exception
        ) {

        }

        bluetoothGatt = null

        isConnecting = true
        isConnected = false

        updateStatus(
            "Connecting..."
        )

        bluetoothGatt =
            device.connectGatt(
                this,
                false,
                gattCallback
            )
    }

    // ----------------------------------------------------
    // GATT callback
    // ----------------------------------------------------

    private val gattCallback =
        object : BluetoothGattCallback() {

            @SuppressLint("MissingPermission")
            override fun onConnectionStateChange(
                gatt: BluetoothGatt,
                status: Int,
                newState: Int
            ) {

                if (
                    newState ==
                    BluetoothProfile.STATE_CONNECTED
                ) {

                    isConnecting = false
                    isConnected = true

                    cancelReconnect()

                    updateStatus(
                        "Connected"
                    )

                    gatt.discoverServices()

                } else if (
                    newState ==
                    BluetoothProfile.STATE_DISCONNECTED
                ) {

                    isConnecting = false
                    isConnected = false

                    try {

                        gatt.close()

                    } catch (
                        e: Exception
                    ) {

                    }

                    if (
                        bluetoothGatt === gatt
                    ) {

                        bluetoothGatt = null
                    }

                    /*
                     * If Bluetooth itself is still ON,
                     * this is probably range, ESP32 power,
                     * interference, etc.
                     */

                    if (
                        bluetoothManager.adapter.isEnabled
                    ) {

                        updateStatus(
                            "Disconnected - reconnecting..."
                        )

                        scheduleReconnect()

                    } else {

                        updateStatus(
                            "Bluetooth is turned off"
                        )
                    }
                }
            }

            @SuppressLint("MissingPermission")
            override fun onServicesDiscovered(
                gatt: BluetoothGatt,
                status: Int
            ) {

                val service =
                    gatt.getService(
                        SERVICE_UUID
                    )

                val characteristic =
                    service?.getCharacteristic(
                        ALARM_UUID
                    )

                if (
                    characteristic != null
                ) {

                    enableNotifications(
                        gatt,
                        characteristic
                    )

                    updateStatus(
                        "Connected and Monitoring"
                    )

                } else {

                    updateStatus(
                        "Alarm service not found"
                    )
                }
            }

            @Deprecated(
                "Used for older Android versions"
            )
            override fun onCharacteristicChanged(
                gatt: BluetoothGatt,
                characteristic:
                    BluetoothGattCharacteristic
            ) {

                if (
                    characteristic.uuid ==
                    ALARM_UUID
                ) {

                    processAlarmValue(
                        characteristic.value
                    )
                }
            }

            override fun onCharacteristicChanged(
                gatt: BluetoothGatt,
                characteristic:
                    BluetoothGattCharacteristic,
                value: ByteArray
            ) {

                if (
                    characteristic.uuid ==
                    ALARM_UUID
                ) {

                    processAlarmValue(
                        value
                    )
                }
            }
        }

    // ----------------------------------------------------
    // Enable notifications from ESP32
    // ----------------------------------------------------

    @SuppressLint("MissingPermission")
    private fun enableNotifications(
        gatt: BluetoothGatt,
        characteristic:
            BluetoothGattCharacteristic
    ) {

        gatt.setCharacteristicNotification(
            characteristic,
            true
        )

        val descriptor =
            characteristic.getDescriptor(
                CCCD_UUID
            )

        if (
            descriptor != null
        ) {

            if (
                Build.VERSION.SDK_INT >=
                Build.VERSION_CODES.TIRAMISU
            ) {

                gatt.writeDescriptor(
                    descriptor,
                    BluetoothGattDescriptor
                        .ENABLE_NOTIFICATION_VALUE
                )

            } else {

                @Suppress("DEPRECATION")
                descriptor.value =
                    BluetoothGattDescriptor
                        .ENABLE_NOTIFICATION_VALUE

                @Suppress("DEPRECATION")
                gatt.writeDescriptor(
                    descriptor
                )
            }
        }
    }

    // ----------------------------------------------------
    // Receive alarm value
    // ----------------------------------------------------

    private fun processAlarmValue(
        value: ByteArray
    ) {

        if (
            value.isEmpty()
        ) {

            return
        }

        val newAlarmState =
            value[0].toInt() == 1

        if (
            newAlarmState ==
            alarmActive
        ) {

            return
        }

        alarmActive =
            newAlarmState

        if (
            alarmActive
        ) {

            sendStatusBroadcast(
                "Smoke Detected",
                true
            )

            startAlarm()

        } else {

            sendStatusBroadcast(
                "Connected and Monitoring",
                false
            )

            stopAlarm()
        }
    }

    // ----------------------------------------------------
    // Alarm ON
    // ----------------------------------------------------

    private fun startAlarm() {

        val vibrator =
            getSystemService(
                Vibrator::class.java
            )

        val pattern =
            longArrayOf(
                0,
                800,
                300,
                800,
                300
            )

        vibrator?.vibrate(
            VibrationEffect.createWaveform(
                pattern,
                0
            )
        )

        val notification =
            NotificationCompat.Builder(
                this,
                ALARM_CHANNEL_ID
            )
                .setSmallIcon(
                    android.R.drawable.ic_dialog_alert
                )
                .setContentTitle(
                    "SAFER SIGNAL"
                )
                .setContentText(
                    "Smoke alarm detected! Follow your emergency plan."
                )
                .setStyle(
                    NotificationCompat.BigTextStyle()
                        .bigText(
                            "Smoke alarm detected! Leave the area immediately and follow your emergency plan."
                        )
                )
                .setPriority(
                    NotificationCompat.PRIORITY_MAX
                )
                .setCategory(
                    NotificationCompat.CATEGORY_ALARM
                )
                .setOngoing(true)
                .setAutoCancel(false)
                .build()

        val manager =
            getSystemService(
                NotificationManager::class.java
            )

        manager.notify(
            ALARM_NOTIFICATION_ID,
            notification
        )
    }

    // ----------------------------------------------------
    // Alarm OFF
    // ----------------------------------------------------

    private fun stopAlarm() {

        val vibrator =
            getSystemService(
                Vibrator::class.java
            )

        vibrator?.cancel()

        val manager =
            getSystemService(
                NotificationManager::class.java
            )

        manager.cancel(
            ALARM_NOTIFICATION_ID
        )
    }

    // ----------------------------------------------------
    // Reconnection
    // ----------------------------------------------------

    private fun scheduleReconnect() {

        if (
            isConnected ||
            isConnecting
        ) {

            return
        }

        /*
         * If Bluetooth itself is OFF,
         * don't repeatedly retry.
         *
         * BluetoothStateReceiver will restart
         * the connection when Bluetooth turns ON.
         */

        if (
            !bluetoothManager.adapter.isEnabled
        ) {

            return
        }

        cancelReconnect()

        reconnectRunnable =
            Runnable {

                if (
                    hasBluetoothPermissions() &&
                    bluetoothManager.adapter.isEnabled &&
                    !isConnected &&
                    !isConnecting
                ) {

                    connectAutomatically()
                }
            }

        handler.postDelayed(
            reconnectRunnable!!,
            5000
        )
    }

    private fun cancelReconnect() {

        reconnectRunnable?.let {

            handler.removeCallbacks(
                it
            )
        }

        reconnectRunnable = null
    }

    // ----------------------------------------------------
    // UI status
    // ----------------------------------------------------

    private fun updateStatus(
        status: String
    ) {

        updateMonitoringNotification(
            status
        )

        sendStatusBroadcast(
            status,
            alarmActive
        )
    }

    private fun sendStatusBroadcast(
        status: String,
        alarm: Boolean
    ) {

        val intent =
            Intent(
                ACTION_STATUS
            ).apply {

                setPackage(
                    packageName
                )

                putExtra(
                    EXTRA_STATUS,
                    status
                )

                putExtra(
                    EXTRA_ALARM,
                    alarm
                )
            }

        sendBroadcast(
            intent
        )
    }

    // ----------------------------------------------------
    // Service shutdown
    // ----------------------------------------------------

    @SuppressLint("MissingPermission")
    override fun onDestroy() {

        super.onDestroy()

        cancelReconnect()

        stopBleScan()

        try {

            unregisterReceiver(
                bluetoothStateReceiver
            )

        } catch (
            e: Exception
        ) {

        }

        try {

            bluetoothGatt?.disconnect()
            bluetoothGatt?.close()

        } catch (
            e: Exception
        ) {

        }

        bluetoothGatt = null

        isConnecting = false
        isConnected = false
    }
}
