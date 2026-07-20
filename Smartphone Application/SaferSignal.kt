class MainActivity : AppCompatActivity() {

    private lateinit var statusText: TextView
    private lateinit var alertBox: View

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        statusText = findViewById(R.id.statusText)
        alertBox = findViewById(R.id.alertBox)

        // Default state
        updateUI(false)
    }

    fun updateUI(alarmState: Boolean) {
        if (alarmState) {
            statusText.text = "SMOKE DETECTED!"
            alertBox.setBackgroundColor(Color.RED)
            vibratePhone()
        } else {
            statusText.text = "All Clear"
            alertBox.setBackgroundColor(Color.GREEN)
        }
    }

    private fun vibratePhone() {
        val vibrator = getSystemService(VIBRATOR_SERVICE) as Vibrator
        vibrator.vibrate(VibrationEffect.createOneShot(500, VibrationEffect.DEFAULT_AMPLITUDE))
    }
}
