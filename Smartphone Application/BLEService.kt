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
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.ParcelUuid
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

        const val SAFER_SIGNAL_DEVICE_NAME = "Safer Signal"

        // Time (ms) to wait for a GATT connection to complete before
        // declaring the attempt failed and restarting the scan.
        const val CONNECTION_TIMEOUT_MS = 8000L

        // Backoff used when a scan or connection attempt fails, and we
        // need to try again. Kept long enough to avoid Android's BLE
        // scan throttling (5 scans in 30s, then 1 per 30s per app).
        const val RECONNECT_DELAY_MS = 8000L

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
    private var connectionTimeoutRunnable: Runnable? = null

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
                            cancelConnectionTimeout()
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

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {

            val manager =
                getSystemService(NotificationManager::class.java)

            val monitoringChannel =
                NotificationChannel(
                    SERVICE_CHANNEL_ID,
                    "Safer Signal Monitoring",
                    NotificationManager.IMPORTANCE_LOW
                ).apply {
                    description =
                        "Keeps Safer Signal connected and monitoring."
                }

            manager.createNotificationChannel(monitoringChannel)

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

            manager.createNotificationChannel(alarmChannel)
        }
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
    //
    // Note: We deliberately do NOT cache the ESP32's MAC
    // address. The ESP32 regenerates its BLE MAC on every
    // power cycle, so a previously saved address becomes
    // stale as soon as the listener is unplugged. We always
    // discover the device by scanning for its advertised
    // name instead.
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

        startScan()
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

        /*
         * Filter by the advertised service UUID. This is
         * cheaper than matching by name and much less likely
         * to be throttled by Android, since the scanner only
         * wakes us for devices that advertise the Safer
         * Signal service.
         */
        val filter =
            ScanFilter.Builder()
                .setServiceUuid(
                    ParcelUuid(SERVICE_UUID)
                )
                .build()

        val settings =
            ScanSettings.Builder()
                .setScanMode(
                    ScanSettings.SCAN_MODE_LOW_LATENCY
                )
                .build()

        try {

            scanner.startScan(
                listOf(filter),
                settings,
                scanCallback
            )

        } catch (_: Exception) {

            isScanning = false

            updateStatus(
                "Bluetooth scan failed"
            )

            scheduleReconnect()
        }
    }

    private val scanCallback =
        object : ScanCallback() {

            @SuppressLint("MissingPermission")
            override fun onScanResult(
                callbackType: Int,
                result: ScanResult
            ) {

                /*
                 * Defense in depth: verify we still hold the
                 * Bluetooth permissions before touching
                 * anything that requires them. Permissions can
                 * be revoked while the service is running.
                 */
                if (!hasBluetoothPermissions()) {

                    updateStatus(
                        "Bluetooth permission required"
                    )

                    return
                }

                val device =
                    result.device

                /*
                 * Double-check by name as well, in case some
                 * other device happens to advertise the same
                 * service UUID during testing. The service
                 * UUID filter is the primary matcher.
                 *
                 * Reading device.name requires BLUETOOTH_CONNECT
                 * on Android 12+. We've already verified the
                 * permission above, and we still catch
                 * SecurityException as a final safety net.
                 */
                val deviceName =
                    try {

                        device.name

                    } catch (
                        _: SecurityException
                    ) {

                        null
                    }

                if (
                    deviceName != null &&
                    deviceName != SAFER_SIGNAL_DEVICE_NAME
                ) {

                    return
                }

                stopBleScan()

                updateStatus(
                    "Safer Signal found"
                )

                connectToDevice(
                    device
                )
            }

            @SuppressLint("MissingPermission")
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
            _: Exception
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
        cancelConnectionTimeout()

        try {

            bluetoothGatt?.close()

        } catch (
            _: Exception
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

        /*
         * If the connection does not complete within the
         * timeout, treat it as a failure and restart the
         * scan. This handles the case where Android silently
         * drops a connectGatt() attempt (e.g. the device
         * disappeared between the scan result and the
         * connection request).
         */
        startConnectionTimeout()
    }

    // ----------------------------------------------------
    // Connection timeout helpers
    // ----------------------------------------------------

    @SuppressLint("MissingPermission")
    private fun startConnectionTimeout() {

        cancelConnectionTimeout()

        connectionTimeoutRunnable =
            Runnable {

                if (isConnecting && !isConnected) {

                    isConnecting = false

                    /*
                     * Snapshot the GATT into a local val so we
                     * can null-check it once and then call
                     * close() on a stable reference. Each
                     * Bluetooth call is wrapped in its own
                     * try/catch(SecurityException), which is
                     * the pattern lint's dataflow analysis
                     * recognizes for permissioned calls.
                     */
                    val gatt =
                        bluetoothGatt

                    if (gatt != null) {

                        try {

                            gatt.close()

                        } catch (
                            _: SecurityException
                        ) {

                            // Permission was revoked during the
                            // wait. Nothing more we can do here.

                        } catch (
                            _: Exception
                        ) {

                            // GATT may already be closed.
                        }
                    }

                    bluetoothGatt = null

                    updateStatus(
                        "Connection timed out - retrying..."
                    )

                    scheduleReconnect()
                }
            }

        handler.postDelayed(
            connectionTimeoutRunnable!!,
            CONNECTION_TIMEOUT_MS
        )
    }

    private fun cancelConnectionTimeout() {

        connectionTimeoutRunnable?.let {
            handler.removeCallbacks(it)
        }

        connectionTimeoutRunnable = null
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

                    cancelConnectionTimeout()

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

                    cancelConnectionTimeout()

                    isConnecting = false
                    isConnected = false

                    try {

                        gatt.close()

                    } catch (
                        _: Exception
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
            getSystemService(Vibrator::class.java)

        val pattern = longArrayOf(
            0, 1200, 200, 1200, 200, 1200
        )

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {

            vibrator?.vibrate(
                VibrationEffect.createWaveform(pattern, 0)
            )

        } else {

            @Suppress("DEPRECATION")
            vibrator?.vibrate(pattern, 0)
        }

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

    @SuppressLint("MissingPermission")
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
            RECONNECT_DELAY_MS
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
        cancelConnectionTimeout()

        stopBleScan()

        try {

            unregisterReceiver(
                bluetoothStateReceiver
            )

        } catch (
            _: Exception
        ) {

        }

        /*
         * Each Bluetooth call is wrapped in its own
         * try/catch(SecurityException) block. Lint's
         * dataflow analysis accepts this pattern because
         * the SecurityException catch is directly adjacent
         * to the call it guards.
         */

        val gatt =
            bluetoothGatt

        if (gatt != null) {

            try {

                gatt.disconnect()

            } catch (
                _: SecurityException
            ) {

                // Permission revoked during teardown.

            } catch (
                _: Exception
            ) {

                // GATT already disconnected.
            }

            try {

                gatt.close()

            } catch (
                _: SecurityException
            ) {

                // Permission revoked during teardown.

            } catch (
                _: Exception
            ) {

                // GATT already closed.
            }
        }

        bluetoothGatt = null

        isConnecting = false
        isConnected = false
    }
}
