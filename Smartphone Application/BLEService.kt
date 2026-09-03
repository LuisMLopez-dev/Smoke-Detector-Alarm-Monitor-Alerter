package com.example.safersignalapp

import android.Manifest
import android.annotation.SuppressLint
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.Intent
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

    private val handler =
        Handler(Looper.getMainLooper())

    private lateinit var bluetoothManager:
            BluetoothManager

    private var bluetoothGatt:
            BluetoothGatt? = null

    private var reconnectRunnable:
            Runnable? = null

    private var alarmActive = false

    override fun onCreate() {
        super.onCreate()

        bluetoothManager =
            getSystemService(
                BluetoothManager::class.java
            )

        createNotificationChannels()

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

        connectAutomatically()

        return START_STICKY
    }

    override fun onBind(
        intent: Intent?
    ): IBinder? = null

    private fun createNotificationChannels() {

        val notificationManager =
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

        notificationManager.createNotificationChannel(
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

        notificationManager.createNotificationChannel(
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
            buildMonitoringNotification(message)
        )
    }

    private fun hasBluetoothPermissions():
            Boolean {

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

    @SuppressLint("MissingPermission")
    private fun connectAutomatically() {

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
                    bluetoothManager.adapter
                        .getRemoteDevice(
                            savedAddress
                        )

                updateStatus(
                    "Reconnecting..."
                )

                connectToDevice(
                    device
                )

            } catch (e: Exception) {

                startScan()
            }

        } else {

            startScan()
        }
    }

    @SuppressLint("MissingPermission")
    private fun startScan() {

        val adapter =
            bluetoothManager.adapter

        if (!adapter.isEnabled) {

            updateStatus(
                "Bluetooth is turned off"
            )

            scheduleReconnect()

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

                val name =
                    try {
                        device.name
                    } catch (e: SecurityException) {
                        null
                    }

                if (
                    name ==
                    SAFER_SIGNAL_DEVICE_NAME
                ) {

                    bluetoothManager.adapter
                        .bluetoothLeScanner
                        ?.stopScan(this)

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

                updateStatus(
                    "Bluetooth scan failed"
                )

                scheduleReconnect()
            }
        }

    @SuppressLint("MissingPermission")
    private fun connectToDevice(
        device: BluetoothDevice
    ) {

        bluetoothGatt?.close()

        bluetoothGatt =
            null

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

    private val gattCallback =
        object :
            BluetoothGattCallback() {

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

                    updateStatus(
                        "Connected"
                    )

                    gatt.discoverServices()

                } else if (
                    newState ==
                    BluetoothProfile.STATE_DISCONNECTED
                ) {

                    updateStatus(
                        "Disconnected - reconnecting..."
                    )

                    scheduleReconnect()
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

                if (characteristic != null) {

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

        if (descriptor != null) {

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

    private fun processAlarmValue(
        value: ByteArray
    ) {

        if (value.isEmpty()) {
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

        sendStatusBroadcast(
            if (alarmActive) {
                "Smoke Detected"
            } else {
                "Connected and Monitoring"
            },
            alarmActive
        )

        if (alarmActive) {

            startAlarm()

        } else {

            stopAlarm()
        }
    }

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

    private fun scheduleReconnect() {

        reconnectRunnable?.let {
            handler.removeCallbacks(it)
        }

        reconnectRunnable =
            Runnable {

                if (
                    hasBluetoothPermissions()
                ) {

                    connectAutomatically()
                }
            }

        handler.postDelayed(
            reconnectRunnable!!,
            5000
        )
    }

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

    @SuppressLint("MissingPermission")
    override fun onDestroy() {
        super.onDestroy()

        reconnectRunnable?.let {
            handler.removeCallbacks(it)
        }

        try {

            bluetoothManager.adapter
                .bluetoothLeScanner
                ?.stopScan(
                    scanCallback
                )

        } catch (e: Exception) {
        }

        bluetoothGatt?.disconnect()
        bluetoothGatt?.close()

        bluetoothGatt = null
    }
}
