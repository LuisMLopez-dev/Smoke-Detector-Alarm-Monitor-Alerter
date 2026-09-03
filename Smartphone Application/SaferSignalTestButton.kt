package com.example.safersignalapp

import android.Manifest
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
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
import androidx.core.content.ContextCompat

class MainActivity :
    ComponentActivity() {

    override fun onCreate(
        savedInstanceState: Bundle?
    ) {
        super.onCreate(
            savedInstanceState
        )

        setContent {
            SaferSignalScreen()
        }
    }
}

@Composable
fun SaferSignalScreen() {

    val context =
        LocalContext.current

    var alarmDetected by remember {
        mutableStateOf(false)
    }

    var connectionStatus by remember {
        mutableStateOf(
            "Starting..."
        )
    }

    var permissionsReady by remember {
        mutableStateOf(false)
    }

    val permissionLauncher =
        rememberLauncherForActivityResult(
            ActivityResultContracts
                .RequestMultiplePermissions()
        ) {

            permissionsReady =
                checkRequiredPermissions(
                    context
                )

            if (permissionsReady) {
                startBleService(
                    context
                )
            }
        }

    DisposableEffect(Unit) {

        val receiver =
            object :
                BroadcastReceiver() {

                override fun onReceive(
                    context: Context?,
                    intent: Intent?
                ) {

                    if (
                        intent?.action ==
                        SaferSignalBleService
                            .ACTION_STATUS
                    ) {

                        connectionStatus =
                            intent.getStringExtra(
                                SaferSignalBleService
                                    .EXTRA_STATUS
                            )
                                ?: "Unknown"

                        alarmDetected =
                            intent.getBooleanExtra(
                                SaferSignalBleService
                                    .EXTRA_ALARM,
                                false
                            )
                    }
                }
            }

        val filter =
            IntentFilter(
                SaferSignalBleService
                    .ACTION_STATUS
            )

        if (
            Build.VERSION.SDK_INT >=
            Build.VERSION_CODES.TIRAMISU
        ) {

            context.registerReceiver(
                receiver,
                filter,
                Context.RECEIVER_NOT_EXPORTED
            )

        } else {

            @Suppress("DEPRECATION")
            context.registerReceiver(
                receiver,
                filter
            )
        }

        onDispose {

            context.unregisterReceiver(
                receiver
            )
        }
    }

    LaunchedEffect(Unit) {

        permissionsReady =
            checkRequiredPermissions(
                context
            )

        if (permissionsReady) {

            startBleService(
                context
            )

        } else {

            val permissions =
                mutableListOf<String>()

            if (
                Build.VERSION.SDK_INT >=
                Build.VERSION_CODES.S
            ) {

                permissions.add(
                    Manifest.permission
                        .BLUETOOTH_SCAN
                )

                permissions.add(
                    Manifest.permission
                        .BLUETOOTH_CONNECT
                )
            }

            if (
                Build.VERSION.SDK_INT >=
                Build.VERSION_CODES.TIRAMISU
            ) {

                permissions.add(
                    Manifest.permission
                        .POST_NOTIFICATIONS
                )
            }

            permissionLauncher.launch(
                permissions.toTypedArray()
            )
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
            .background(
                backgroundColor
            )
            .padding(
                28.dp
            ),

        horizontalAlignment =
            Alignment.CenterHorizontally,

        verticalArrangement =
            Arrangement.Center
    ) {

        Text(
            text =
                "SAFER SIGNAL",

            fontSize =
                28.sp,

            fontWeight =
                FontWeight.Bold,

            color =
                Color.White
        )

        Spacer(
            modifier =
                Modifier.height(
                    30.dp
                )
        )

        Text(
            text =
                connectionStatus,

            fontSize =
                18.sp,

            color =
                Color.White,

            textAlign =
                TextAlign.Center
        )

        Spacer(
            modifier =
                Modifier.height(
                    45.dp
                )
        )

        Text(
            text =
                statusText,

            fontSize =
                52.sp,

            fontWeight =
                FontWeight.Bold,

            color =
                Color.White,

            textAlign =
                TextAlign.Center,

            lineHeight =
                58.sp
        )

        Spacer(
            modifier =
                Modifier.height(
                    30.dp
                )
        )

        Text(
            text =
                instructionText,

            fontSize =
                22.sp,

            color =
                Color.White,

            textAlign =
                TextAlign.Center,

            lineHeight =
                30.sp
        )

        Spacer(
            modifier =
                Modifier.height(
                    55.dp
                )
        )

        if (!permissionsReady) {

            Button(
                onClick = {

                    val permissions =
                        mutableListOf<String>()

                    if (
                        Build.VERSION.SDK_INT >=
                        Build.VERSION_CODES.S
                    ) {

                        permissions.add(
                            Manifest.permission
                                .BLUETOOTH_SCAN
                        )

                        permissions.add(
                            Manifest.permission
                                .BLUETOOTH_CONNECT
                        )
                    }

                    if (
                        Build.VERSION.SDK_INT >=
                        Build.VERSION_CODES.TIRAMISU
                    ) {

                        permissions.add(
                            Manifest.permission
                                .POST_NOTIFICATIONS
                        )
                    }

                    permissionLauncher.launch(
                        permissions.toTypedArray()
                    )
                },

                modifier =
                    Modifier
                        .fillMaxWidth()
                        .height(
                            65.dp
                        ),

                colors =
                    ButtonDefaults
                        .buttonColors(
                            containerColor =
                                Color.White
                        )
            ) {

                Text(
                    text =
                        "Enable Safer Signal",

                    color =
                        Color.Black,

                    fontSize =
                        20.sp,

                    fontWeight =
                        FontWeight.Bold
                )
            }
        }
    }
}

fun checkRequiredPermissions(
    context: Context
): Boolean {

    if (
        Build.VERSION.SDK_INT >=
        Build.VERSION_CODES.S
    ) {

        val scan =
            ContextCompat
                .checkSelfPermission(
                    context,
                    Manifest.permission
                        .BLUETOOTH_SCAN
                )

        val connect =
            ContextCompat
                .checkSelfPermission(
                    context,
                    Manifest.permission
                        .BLUETOOTH_CONNECT
                )

        if (
            scan !=
            PackageManager.PERMISSION_GRANTED ||
            connect !=
            PackageManager.PERMISSION_GRANTED
        ) {

            return false
        }
    }

    if (
        Build.VERSION.SDK_INT >=
        Build.VERSION_CODES.TIRAMISU
    ) {

        val notifications =
            ContextCompat
                .checkSelfPermission(
                    context,
                    Manifest.permission
                        .POST_NOTIFICATIONS
                )

        if (
            notifications !=
            PackageManager.PERMISSION_GRANTED
        ) {

            return false
        }
    }

    return true
}

fun startBleService(
    context: Context
) {

    val intent =
        Intent(
            context,
            SaferSignalBleService::class.java
        )

    ContextCompat.startForegroundService(
        context,
        intent
    )
}
