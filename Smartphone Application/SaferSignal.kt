package com.example.safersignalapp

import android.Manifest
import android.annotation.SuppressLint
import android.app.NotificationChannel
import android.app.NotificationManager
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
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.VibrationEffect
import android.os.Vibrator
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import java.util.UUID

const val SAFER_SIGNAL_DEVICE_NAME = "Safer Signal"

val SAFER_SIGNAL_SERVICE_UUID: UUID =
    UUID.fromString("12345678-1234-1234-1234-123456789001")

val ALARM_CHARACTERISTIC_UUID: UUID =
    UUID.fromString("12345678-1234-1234-1234-123456789002")

val CCCD_UUID: UUID =
    UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

const val PREFS_NAME = "SaferSignalPrefs"
const val SAVED_DEVICE_ADDRESS = "savedDeviceAddress"

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        createNotificationChannel()

        setContent {
            SaferSignalScreen()
        }
    }

    private fun createNotificationChannel() {

        val channel = NotificationChannel(
            "safer_signal_alarm",
            "Safer Signal Emergency Alerts",
            NotificationManager.IMPORTANCE_HIGH
        ).apply {
            description = "Emergency smoke alarm notifications"
            enableVibration(true)
        }

        val notificationManager =
            getSystemService(NotificationManager::class.java)

        notificationManager.createNotificationChannel(channel)
    }
}

@SuppressLint("MissingPermission")
@Composable
fun SaferSignalScreen() {

    val context = LocalContext.current

    var alarmDetected by remember {
        mutableStateOf(false)
    }

    var connectionStatus by remember {
        mutableStateOf("Starting...")
    }

    var hasSavedDevice by remember {
        mutableStateOf(false)
    }

    val bleClient = remember {

        SaferSignalBleClient(
            context = context,

            onConnectionChange = {
                connectionStatus = it
            },

            onAlarmChange = {
                alarmDetected = it
            },

            onDeviceSaved = {
                hasSavedDevice = true
            }
        )
    }

    val permissionLauncher =
        rememberLauncherForActivityResult(
            ActivityResultContracts.RequestMultiplePermissions()
        ) {

            if (checkRequiredPermissions(context)) {

                if (bleClient.hasSavedDevice()) {
                    bleClient.connectToSavedDevice()
                } else {
                    bleClient.scanAndConnect()
                }
            }
        }

    LaunchedEffect(Unit) {

        hasSavedDevice =
            bleClient.hasSavedDevice()

        if (checkRequiredPermissions(context)) {

            if (hasSavedDevice) {
                bleClient.connectToSavedDevice()
            } else {
                connectionStatus = "Not Paired"
            }

        } else {

            val permissions =
                mutableListOf<String>()

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {

                permissions.add(
                    Manifest.permission.BLUETOOTH_SCAN
                )

                permissions.add(
                    Manifest.permission.BLUETOOTH_CONNECT
                )
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {

                permissions.add(
                    Manifest.permission.POST_NOTIFICATIONS
                )
            }

            permissionLauncher.launch(
                permissions.toTypedArray()
            )
        }
    }

    LaunchedEffect(alarmDetected) {

        val vibrator =
            context.getSystemService(Vibrator::class.java)

        if (alarmDetected) {

            val pattern = longArrayOf(
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

            showAlarmNotification(context)

        } else {

            vibrator?.cancel()
            cancelAlarmNotification(context)
        }
    }

    DisposableEffect(Unit) {

        onDispose {
            bleClient.disconnect()
        }
    }

    val backgroundColor =
        if (alarmDetected) {
            Color(0xFFC62828)
        } else {
            Color(0xFF2E7D32)
        }

    val statusText =
        if (alarmDetected) {
            "SMOKE\nDETECTED!"
        } else {
            "ALL CLEAR"
        }

    val instructionText =
        if (alarmDetected) {
            "Leave the area and follow your emergency plan."
        } else {
            "Safer Signal is monitoring for a smoke alarm."
        }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(backgroundColor)
            .padding(28.dp),

        horizontalAlignment = Alignment.CenterHorizontally,

        verticalArrangement = Arrangement.Center
    ) {

        Text(
            text = "SAFER SIGNAL",
            fontSize = 28.sp,
            fontWeight = FontWeight.Bold,
            color = Color.White
        )

        Spacer(
            modifier = Modifier.height(30.dp)
        )

        Text(
            text = connectionStatus,
            fontSize = 18.sp,
            color = Color.White,
            textAlign = TextAlign.Center
        )

        Spacer(
            modifier = Modifier.height(45.dp)
        )

        Text(
            text = statusText,
            fontSize = 52.sp,
            fontWeight = FontWeight.Bold,
            color = Color.White,
            textAlign = TextAlign.Center,
            lineHeight = 58.sp
        )

        Spacer(
            modifier = Modifier.height(30.dp)
        )

        Text(
            text = instructionText,
            fontSize = 22.sp,
            color = Color.White,
            textAlign = TextAlign.Center,
            lineHeight = 30.sp
        )

        Spacer(
            modifier = Modifier.height(55.dp)
        )

        if (!hasSavedDevice) {

            Button(
                onClick = {

                    if (checkRequiredPermissions(context)) {

                        bleClient.scanAndConnect()

                    } else {

                        val permissions =
                            mutableListOf<String>()

                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {

                            permissions.add(
                                Manifest.permission.BLUETOOTH_SCAN
                            )

                            permissions.add(
                                Manifest.permission.BLUETOOTH_CONNECT
                            )
                        }

                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {

                            permissions.add(
                                Manifest.permission.POST_NOTIFICATIONS
                            )
                        }

                        permissionLauncher.launch(
                            permissions.toTypedArray()
                        )
                    }
                },

                modifier = Modifier
                    .fillMaxWidth()
                    .height(65.dp),

                colors = ButtonDefaults.buttonColors(
                    containerColor = Color.White
                )
            ) {

                Text(
                    text = "Connect Safer Signal",
                    color = Color.Black,
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold
                )
            }

            Spacer(
                modifier = Modifier.height(20.dp)
            )
        }

        Button(
            onClick = {
                alarmDetected = !alarmDetected
            },

            modifier = Modifier
                .fillMaxWidth()
                .height(65.dp),

            colors = ButtonDefaults.buttonColors(
                containerColor = Color.White
            )
        ) {

            Text(
                text =
                    if (alarmDetected) {
                        "Stop Test Alarm"
                    } else {
                        "Test Alarm"
                    },

                color = Color.Black,
                fontSize = 20.sp,
                fontWeight = FontWeight.Bold
            )
        }
    }
}

fun checkRequiredPermissions(
    context: Context
): Boolean {

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {

        val scanPermission =
            ContextCompat.checkSelfPermission(
                context,
                Manifest.permission.BLUETOOTH_SCAN
            )

        val connectPermission =
            ContextCompat.checkSelfPermission(
                context,
                Manifest.permission.BLUETOOTH_CONNECT
            )

        if (
            scanPermission != PackageManager.PERMISSION_GRANTED ||
            connectPermission != PackageManager.PERMISSION_GRANTED
        ) {
            return false
        }
    }

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {

        val notificationPermission =
            ContextCompat.checkSelfPermission(
                context,
                Manifest.permission.POST_NOTIFICATIONS
            )

        if (
            notificationPermission != PackageManager.PERMISSION_GRANTED
        ) {
            return false
        }
    }

    return true
}

@SuppressLint("MissingPermission")
fun showAlarmNotification(
    context: Context
) {

    val notification =
        NotificationCompat.Builder(
            context,
            "safer_signal_alarm"
        )
            .setSmallIcon(
                android.R.drawable.ic_dialog_alert
            )
            .setContentTitle(
                "SAFER SIGNAL"
            )
            .setContentText(
                "Smoke alarm detected! Leave the area immediately."
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

    val notificationManager =
        context.getSystemService(
            NotificationManager::class.java
        )

    notificationManager.notify(
        1001,
        notification
    )
}

fun cancelAlarmNotification(
    context: Context
) {

    val notificationManager =
        context.getSystemService(
            NotificationManager::class.java
        )

    notificationManager.cancel(
        1001
    )
}

@SuppressLint("MissingPermission")
class SaferSignalBleClient(

    private val context: Context,

    private val onConnectionChange:
        (String) -> Unit,

    private val onAlarmChange:
        (Boolean) -> Unit,

    private val onDeviceSaved:
        () -> Unit

) {

    private val bluetoothManager =
        context.getSystemService(
            BluetoothManager::class.java
        )

    private val bluetoothAdapter =
        bluetoothManager.adapter

    private val preferences =
        context.getSharedPreferences(
            PREFS_NAME,
            Context.MODE_PRIVATE
        )

    private var bluetoothGatt:
            BluetoothGatt? = null

    fun hasSavedDevice(): Boolean {

        return preferences.contains(
            SAVED_DEVICE_ADDRESS
        )
    }

    fun connectToSavedDevice() {

        val savedAddress =
            preferences.getString(
                SAVED_DEVICE_ADDRESS,
                null
            )

        if (savedAddress == null) {

            onConnectionChange(
                "Not Paired"
            )

            return
        }

        try {

            val device =
                bluetoothAdapter.getRemoteDevice(
                    savedAddress
                )

            onConnectionChange(
                "Reconnecting..."
            )

            connectToDevice(
                device
            )

        } catch (e: Exception) {

            onConnectionChange(
                "Unable to reconnect"
            )
        }
    }

    fun scanAndConnect() {

        if (!bluetoothAdapter.isEnabled) {

            onConnectionChange(
                "Bluetooth is turned off"
            )

            return
        }

        val scanner =
            bluetoothAdapter.bluetoothLeScanner

        if (scanner == null) {

            onConnectionChange(
                "BLE scanner unavailable"
            )

            return
        }

        onConnectionChange(
            "Searching for Safer Signal..."
        )

        scanner.startScan(
            scanCallback
        )
    }

    private val scanCallback =
        object : ScanCallback() {

            override fun onScanResult(
                callbackType: Int,
                result: ScanResult
            ) {

                val device =
                    result.device

                val deviceName =
                    try {
                        device.name
                    } catch (e: SecurityException) {
                        null
                    }

                if (
                    deviceName ==
                    SAFER_SIGNAL_DEVICE_NAME
                ) {

                    bluetoothAdapter
                        .bluetoothLeScanner
                        ?.stopScan(this)

                    preferences
                        .edit()
                        .putString(
                            SAVED_DEVICE_ADDRESS,
                            device.address
                        )
                        .apply()

                    onDeviceSaved()

                    onConnectionChange(
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

                onConnectionChange(
                    "Bluetooth scan failed"
                )
            }
        }

    private fun connectToDevice(
        device: BluetoothDevice
    ) {

        onConnectionChange(
            "Connecting..."
        )

        bluetoothGatt =
            device.connectGatt(
                context,
                false,
                gattCallback
            )
    }

    private val gattCallback =
        object : BluetoothGattCallback() {

            override fun onConnectionStateChange(
                gatt: BluetoothGatt,
                status: Int,
                newState: Int
            ) {

                if (
                    newState ==
                    BluetoothProfile.STATE_CONNECTED
                ) {

                    onConnectionChange(
                        "Connected"
                    )

                    gatt.discoverServices()

                } else if (
                    newState ==
                    BluetoothProfile.STATE_DISCONNECTED
                ) {

                    onConnectionChange(
                        "Reconnecting..."
                    )

                    connectToSavedDevice()
                }
            }

            override fun onServicesDiscovered(
                gatt: BluetoothGatt,
                status: Int
            ) {

                val service =
                    gatt.getService(
                        SAFER_SIGNAL_SERVICE_UUID
                    )

                val characteristic =
                    service?.getCharacteristic(
                        ALARM_CHARACTERISTIC_UUID
                    )

                if (
                    characteristic != null
                ) {

                    enableAlarmNotifications(
                        gatt,
                        characteristic
                    )

                    onConnectionChange(
                        "Connected and Monitoring"
                    )

                } else {

                    onConnectionChange(
                        "Alarm service not found"
                    )
                }
            }

            @Deprecated(
                "Used for compatibility"
            )
            override fun onCharacteristicChanged(
                gatt: BluetoothGatt,
                characteristic:
                BluetoothGattCharacteristic
            ) {

                if (
                    characteristic.uuid ==
                    ALARM_CHARACTERISTIC_UUID
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
                    ALARM_CHARACTERISTIC_UUID
                ) {

                    processAlarmValue(
                        value
                    )
                }
            }
        }

    private fun enableAlarmNotifications(
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
                    BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
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

        if (value.isNotEmpty()) {

            val alarmActive =
                value[0].toInt() == 1

            onAlarmChange(
                alarmActive
            )
        }
    }

    fun disconnect() {

        try {

            bluetoothAdapter
                .bluetoothLeScanner
                ?.stopScan(
                    scanCallback
                )

        } catch (e: Exception) {
            // Scanner may not be running
        }

        bluetoothGatt?.disconnect()
        bluetoothGatt?.close()
        bluetoothGatt = null
    }
}
