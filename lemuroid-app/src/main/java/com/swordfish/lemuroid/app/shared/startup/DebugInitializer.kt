package com.swordfish.lemuroid.app.shared.startup

import android.content.Context
import android.os.StrictMode
import android.util.Log
import androidx.startup.Initializer
import com.swordfish.lemuroid.BuildConfig
import timber.log.Timber

class DebugInitializer : Initializer<Unit> {
    override fun create(context: Context) {
        if (BuildConfig.DEBUG) {
            Timber.plant(Timber.DebugTree())
            enableStrictMode()
        } else {
            // Arcadia : garder les avertissements et erreurs dans le journal (balise « Arcadia »)
            // pour pouvoir diagnostiquer un problème avec « adb logcat -s Arcadia ».
            Timber.plant(JournalArcadia())
        }
    }

    private class JournalArcadia : Timber.Tree() {
        override fun isLoggable(
            tag: String?,
            priority: Int,
        ): Boolean = priority >= Log.WARN

        override fun log(
            priority: Int,
            tag: String?,
            message: String,
            t: Throwable?,
        ) {
            val texte = if (t != null) message + "\n" + Log.getStackTraceString(t) else message
            Log.println(priority, "Arcadia", texte)
        }
    }

    private fun enableStrictMode() {
        StrictMode.setThreadPolicy(
            StrictMode.ThreadPolicy.Builder()
                .detectAll()
                .penaltyLog()
                .build(),
        )
    }

    override fun dependencies(): List<Class<out Initializer<*>>> {
        return emptyList()
    }
}
