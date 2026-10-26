package com.swordfish.lemuroid.app.arcadia.ia

import android.content.Context
import android.speech.tts.TextToSpeech
import java.util.Locale

/** Lecture à voix haute avec la synthèse vocale du téléphone (fonctionne sans Internet). */
class Voix(context: Context) : TextToSpeech.OnInitListener {
    private val tts = TextToSpeech(context.applicationContext, this)
    private var pret = false
    private var enAttente: String? = null
    var langue: Locale = Locale.getDefault()

    override fun onInit(status: Int) {
        pret = status == TextToSpeech.SUCCESS
        if (pret) {
            val resultat = tts.setLanguage(langue)
            if (resultat == TextToSpeech.LANG_MISSING_DATA || resultat == TextToSpeech.LANG_NOT_SUPPORTED) {
                tts.setLanguage(Locale.FRENCH)
            }
            enAttente?.let { parler(it) }
            enAttente = null
        }
    }

    fun parler(texte: String) {
        if (texte.isBlank()) return
        if (!pret) {
            enAttente = texte
            return
        }
        tts.speak(texte, TextToSpeech.QUEUE_FLUSH, null, "arcadia")
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
