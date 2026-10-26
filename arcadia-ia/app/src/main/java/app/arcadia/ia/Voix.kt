package app.arcadia.ia

import android.content.Context
import android.content.Intent
import android.speech.RecognizerIntent
import android.speech.tts.TextToSpeech
import java.util.Locale

/** Lit les réponses à voix haute (synthèse vocale du téléphone, fonctionne hors ligne). */
class LecteurVocal(context: Context) : TextToSpeech.OnInitListener {
    private val tts = TextToSpeech(context.applicationContext, this)
    private var pret = false
    private var enAttente: String? = null

    override fun onInit(status: Int) {
        pret = status == TextToSpeech.SUCCESS
        if (pret) {
            val r = tts.setLanguage(Locale.getDefault())
            if (r == TextToSpeech.LANG_MISSING_DATA || r == TextToSpeech.LANG_NOT_SUPPORTED) tts.setLanguage(Locale.FRENCH)
            enAttente?.let { lire(it) }
            enAttente = null
        }
    }

    fun lire(texte: String) {
        val propre = texte.replace(Regex("[*#`_]"), "").trim()
        if (propre.isEmpty()) return
        if (!pret) {
            enAttente = propre
            return
        }
        tts.speak(propre, TextToSpeech.QUEUE_FLUSH, null, "arcadia-ia")
    }

    fun arreter() {
        if (pret) tts.stop()
    }

    fun fermer() {
        runCatching {
            tts.stop()
            tts.shutdown()
        }
    }
}

/** Demande à Android d'écouter une phrase (dictée vocale du téléphone). */
fun intentionDictee(): Intent =
    Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH).apply {
        putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
        putExtra(RecognizerIntent.EXTRA_LANGUAGE, Locale.getDefault().toLanguageTag())
        putExtra(RecognizerIntent.EXTRA_PROMPT, "Parlez, je vous écoute…")
        putExtra(RecognizerIntent.EXTRA_PREFER_OFFLINE, true)
    }

fun resultatDictee(data: Intent?): String? =
    data?.getStringArrayListExtra(RecognizerIntent.EXTRA_RESULTS)?.firstOrNull { it.isNotBlank() }
