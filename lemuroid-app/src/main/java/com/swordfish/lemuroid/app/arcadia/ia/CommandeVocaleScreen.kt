package com.swordfish.lemuroid.app.arcadia.ia

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.speech.RecognitionListener
import android.speech.RecognizerIntent
import android.speech.SpeechRecognizer
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.MicOff
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import com.swordfish.lemuroid.app.arcadia.ui.ArcadiaCouleurs
import java.util.Locale

/** Écoute une phrase avec la reconnaissance vocale du téléphone (de préférence hors ligne). */
class Ecoute(private val context: Context) : RecognitionListener {
    enum class Etape { PRETE, ECOUTE, REFLEXION, TERMINEE, ERREUR }

    var etape by mutableStateOf(Etape.PRETE)
        private set
    var texte by mutableStateOf("")
        private set
    var message by mutableStateOf<String?>(null)
    var niveau by mutableFloatStateOf(0f)
        private set

    var surPhrase: ((String) -> Unit)? = null

    private val reconnaisseur: SpeechRecognizer? =
        if (SpeechRecognizer.isRecognitionAvailable(context)) {
            SpeechRecognizer.createSpeechRecognizer(context).also { it.setRecognitionListener(this) }
        } else {
            null
        }

    val disponible: Boolean
        get() = reconnaisseur != null

    fun demarrer() {
        val r = reconnaisseur
        if (r == null) {
            etape = Etape.ERREUR
            message = "La reconnaissance vocale n'est pas disponible sur ce téléphone."
            return
        }
        texte = ""
        message = null
        etape = Etape.ECOUTE
        val intent =
            Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH).apply {
                putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
                putExtra(RecognizerIntent.EXTRA_LANGUAGE, Locale.getDefault().toLanguageTag())
                putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true)
                putExtra(RecognizerIntent.EXTRA_PREFER_OFFLINE, true)
                putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, 3)
                putExtra(RecognizerIntent.EXTRA_CALLING_PACKAGE, context.packageName)
            }
        runCatching { r.cancel() }
        r.startListening(intent)
    }

    fun arreter() {
        runCatching { reconnaisseur?.stopListening() }
    }

    fun liberer() {
        runCatching { reconnaisseur?.destroy() }
    }

    override fun onReadyForSpeech(params: Bundle?) {
        etape = Etape.ECOUTE
    }

    override fun onBeginningOfSpeech() = Unit

    override fun onRmsChanged(rmsdB: Float) {
        niveau = ((rmsdB + 2f) / 12f).coerceIn(0f, 1f)
    }

    override fun onBufferReceived(buffer: ByteArray?) = Unit

    override fun onEndOfSpeech() {
        etape = Etape.REFLEXION
        niveau = 0f
    }

    override fun onError(error: Int) {
        niveau = 0f
        etape = Etape.ERREUR
        message =
            when (error) {
                SpeechRecognizer.ERROR_NO_MATCH, SpeechRecognizer.ERROR_SPEECH_TIMEOUT ->
                    "Je n'ai rien entendu. Touchez le micro et parlez."
                SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS ->
                    "Le micro n'est pas autorisé pour Arcadia."
                SpeechRecognizer.ERROR_NETWORK, SpeechRecognizer.ERROR_NETWORK_TIMEOUT ->
                    "La reconnaissance vocale hors ligne n'est pas installée. Dans les paramètres du " +
                        "téléphone, cherchez « reconnaissance vocale hors ligne » et ajoutez le français."
                SpeechRecognizer.ERROR_RECOGNIZER_BUSY -> "Le micro est occupé, réessayez."
                else -> "La reconnaissance vocale a échoué (code $error). Réessayez."
            }
    }

    override fun onResults(results: Bundle?) {
        val phrases = results?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION).orEmpty()
        val phrase = phrases.firstOrNull { it.isNotBlank() }
        if (phrase == null) {
            onError(SpeechRecognizer.ERROR_NO_MATCH)
            return
        }
        texte = phrase
        etape = Etape.TERMINEE
        surPhrase?.invoke(phrase)
    }

    override fun onPartialResults(partialResults: Bundle?) {
        partialResults?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)
            ?.firstOrNull { it.isNotBlank() }
            ?.let { texte = it }
    }

    override fun onEvent(
        eventType: Int,
        params: Bundle?,
    ) = Unit
}

/**
 * Écran « Commande vocale » du menu en jeu.
 * @param onPhrase reçoit la commande comprise ; renvoie un message à afficher si elle n'a pas pu
 * être exécutée (ex. jeu introuvable), ou null si tout s'est bien passé.
 */
@Composable
fun CommandeVocaleScreen(onPhrase: (Commande, String) -> String?) {
    val context = LocalContext.current
    val ecoute = remember { Ecoute(context) }
    DisposableEffect(Unit) {
        onDispose { ecoute.liberer() }
    }

    ecoute.surPhrase = { phrase ->
        val commande = CommandeVocale.interpreter(phrase)
        if (commande is Commande.Inconnue) {
            ecoute.message = "Je n'ai pas compris « $phrase ». Essayez une des phrases ci-dessous."
        } else {
            onPhrase(commande, phrase)?.let { ecoute.message = it }
        }
    }

    val demandeMicro =
        rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { accorde ->
            if (accorde) {
                ecoute.demarrer()
            } else {
                ecoute.message = "Sans accès au micro, la commande vocale ne peut pas fonctionner."
            }
        }

    fun lancer() {
        val autorise =
            ContextCompat.checkSelfPermission(context, Manifest.permission.RECORD_AUDIO) ==
                PackageManager.PERMISSION_GRANTED
        if (autorise) ecoute.demarrer() else demandeMicro.launch(Manifest.permission.RECORD_AUDIO)
    }

    LaunchedEffect(Unit) { lancer() }

    val echelle by animateFloatAsState(
        targetValue = if (ecoute.etape == Ecoute.Etape.ECOUTE) 1f + ecoute.niveau * 0.35f else 1f,
        label = "micro",
    )

    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(20.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Box(
            modifier =
                Modifier
                    .padding(top = 12.dp)
                    .size(120.dp)
                    .scale(echelle)
                    .clip(CircleShape)
                    .background(ArcadiaCouleurs.degradeIA)
                    .clickable { lancer() },
            contentAlignment = Alignment.Center,
        ) {
            Icon(
                imageVector = if (ecoute.etape == Ecoute.Etape.ERREUR) Icons.Filled.MicOff else Icons.Filled.Mic,
                contentDescription = "Parler",
                tint = Color.White,
                modifier = Modifier.size(56.dp),
            )
        }

        Text(
            text =
                when (ecoute.etape) {
                    Ecoute.Etape.PRETE -> "Touchez le micro pour parler"
                    Ecoute.Etape.ECOUTE -> "Je vous écoute…"
                    Ecoute.Etape.REFLEXION -> "Je réfléchis…"
                    Ecoute.Etape.TERMINEE -> "Compris !"
                    Ecoute.Etape.ERREUR -> "Touchez le micro pour réessayer"
                },
            style = MaterialTheme.typography.titleMedium,
            fontWeight = FontWeight.Bold,
        )

        if (ecoute.texte.isNotBlank()) {
            Text(
                text = "« ${ecoute.texte} »",
                style = MaterialTheme.typography.bodyLarge,
                textAlign = TextAlign.Center,
            )
        }

        ecoute.message?.let {
            Text(
                text = it,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.tertiary,
                textAlign = TextAlign.Center,
            )
        }

        Column(
            modifier = Modifier.fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Text(
                text = "Vous pouvez dire :",
                style = MaterialTheme.typography.labelLarge,
                color = MaterialTheme.colorScheme.primary,
            )
            CommandeVocale.exemples.forEach {
                Text(
                    text = "• $it",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}
