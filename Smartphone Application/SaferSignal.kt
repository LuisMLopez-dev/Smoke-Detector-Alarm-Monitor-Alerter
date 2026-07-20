package com.example.safersignalapp

import android.os.Bundle
import android.os.VibrationEffect
import android.os.Vibrator
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.sp

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            AlarmScreen()
        }
    }
}

@Composable
fun AlarmScreen() {

    var alarmState by remember { mutableStateOf(false) }
    val context = LocalContext.current
    
    val backgroundColor = if (alarmState) Color.Red else Color.Green
    val statusText = if (alarmState) "SMOKE DETECTED!" else "All Clear"

    // Trigger vibration when alarm turns ON
    LaunchedEffect(alarmState) {
        if (alarmState) {
            val vibrator = context.getSystemService(Vibrator::class.java)

            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
                vibrator?.vibrate(
                    VibrationEffect.createOneShot(500, VibrationEffect.DEFAULT_AMPLITUDE)
                )
            } else {
                @Suppress("DEPRECATION")
                vibrator?.vibrate(500)
            }
        }
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(backgroundColor),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = statusText,
            fontSize = 32.sp,
            color = Color.White
        )
    }
}
