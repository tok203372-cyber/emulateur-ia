package com.swordfish.lemuroid.app.arcadia.ia

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Rect
import android.util.LruCache
import com.google.android.gms.tasks.Task
import com.google.mlkit.common.MlKit
import com.google.mlkit.nl.languageid.LanguageIdentification
import com.google.mlkit.nl.languageid.LanguageIdentificationOptions
import com.google.mlkit.nl.translate.TranslateLanguage
import com.google.mlkit.nl.translate.Translation
import com.google.mlkit.nl.translate.Translator
import com.google.mlkit.nl.translate.TranslatorOptions
import com.google.mlkit.vision.common.InputImage
import com.google.mlkit.vision.text.TextRecognition
import com.google.mlkit.vision.text.japanese.JapaneseTextRecognizerOptions
import kotlinx.coroutines.suspendCancellableCoroutine
import timber.log.Timber
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

/** Attend la fin d'une tâche ML Kit sans bloquer. */
suspend fun <T> Task<T>.attendre(): T =
    suspendCancellableCoroutine { suite ->
        addOnSuccessListener { if (suite.isActive) suite.resume(it) }
        addOnFailureListener { if (suite.isActive) suite.resumeWithException(it) }
        addOnCanceledListener { suite.cancel() }
    }

/**
 * Lit le texte affiché par le jeu (japonais, anglais, français…) puis le traduit, le tout sur le
 * téléphone, sans Internet (sauf le tout premier téléchargement d'une langue, ~30 Mo).
 */
class LecteurEcran(context: Context) {
    /** Un morceau de texte trouvé sur l'image (coordonnées en pixels de l'image analysée). */
    data class Bloc(val zone: Rect, val texte: String, val lignes: Int, val langue: String)

    data class BlocTraduit(val bloc: Bloc, val traduction: String?)

    private val appContext = context.applicationContext
    private val reconnaissance by lazy {
        initialiserMlKit(appContext)
        TextRecognition.getClient(JapaneseTextRecognizerOptions.Builder().build())
    }
    private val identification by lazy {
        initialiserMlKit(appContext)
        LanguageIdentification.getClient(
            LanguageIdentificationOptions.Builder().setConfidenceThreshold(0.35f).build(),
        )
    }
    private val traducteurs = mutableMapOf<String, Translator>()
    private val modelesPrets = mutableSetOf<String>()
    private val memoire = LruCache<String, String>(500)

    /** Langue vers laquelle traduire (code ML Kit, ex. « fr »). */
    var langueCible: String = ReglagesIA.langueCible(appContext)

    suspend fun lire(image: Bitmap): List<Bloc> {
        val resultat = reconnaissance.process(InputImage.fromBitmap(image, 0)).attendre()
        return resultat.textBlocks.mapNotNull { bloc ->
            val zone = bloc.boundingBox ?: return@mapNotNull null
            val lignes = bloc.lines.map { it.text.trim() }.filter { it.isNotEmpty() }
            if (lignes.isEmpty()) return@mapNotNull null
            val japonais = lignes.any { contientJaponais(it) }
            val texte = if (japonais) lignes.joinToString("") else recoller(lignes)
            if (!estUtile(texte)) return@mapNotNull null
            Bloc(zone, texte, lignes.size, if (japonais) TranslateLanguage.JAPANESE else "")
        }
    }

    /**
     * Traduit les blocs qui ne sont pas déjà dans la langue voulue.
     * @param surTelechargement appelé quand une langue doit d'abord être téléchargée.
     */
    suspend fun traduire(
        blocs: List<Bloc>,
        surTelechargement: (String) -> Unit = {},
    ): List<BlocTraduit> =
        blocs.map { bloc ->
            val source = bloc.langue.ifEmpty { detecterLangue(bloc.texte) }
            if (source == langueCible || source.isEmpty()) {
                BlocTraduit(bloc, null)
            } else {
                BlocTraduit(bloc, traduireTexte(bloc.texte, source, surTelechargement))
            }
        }

    suspend fun traduireTexte(
        texte: String,
        source: String,
        surTelechargement: (String) -> Unit = {},
    ): String? {
        val cle = "$source>$langueCible:$texte"
        memoire.get(cle)?.let { return it }
        val traducteur = traducteur(source) ?: return null
        return runCatching {
            if (source !in modelesPrets) {
                surTelechargement(nomLangue(source))
                traducteur.downloadModelIfNeeded().attendre()
                modelesPrets += source
            }
            traducteur.translate(texte).attendre()
        }.onFailure { Timber.w(it, "Arcadia : traduction impossible ($source)") }
            .getOrNull()
            ?.also { memoire.put(cle, it) }
    }

    private suspend fun detecterLangue(texte: String): String {
        val code =
            runCatching { identification.identifyLanguage(texte).attendre() }
                .getOrDefault("und")
        if (code == "und") {
            // Texte trop court pour être sûr : la plupart des jeux non japonais sont en anglais.
            return if (texte.any { it.isLetter() }) TranslateLanguage.ENGLISH else ""
        }
        return TranslateLanguage.fromLanguageTag(code) ?: ""
    }

    private fun traducteur(source: String): Translator? {
        traducteurs[source]?.let { return it }
        val depuis = TranslateLanguage.fromLanguageTag(source) ?: return null
        val vers = TranslateLanguage.fromLanguageTag(langueCible) ?: TranslateLanguage.FRENCH
        val options = TranslatorOptions.Builder().setSourceLanguage(depuis).setTargetLanguage(vers).build()
        return Translation.getClient(options).also { traducteurs[source] = it }
    }

    fun fermer() {
        traducteurs.values.forEach { runCatching { it.close() } }
        traducteurs.clear()
        runCatching { reconnaissance.close() }
        runCatching { identification.close() }
    }

    companion object {
        @Volatile
        private var mlKitPret = false

        /** ML Kit se prépare tout seul dans le processus principal, mais pas dans celui du jeu. */
        fun initialiserMlKit(context: Context) {
            if (mlKitPret) return
            synchronized(this) {
                if (mlKitPret) return
                runCatching { MlKit.initialize(context.applicationContext) }
                mlKitPret = true
            }
        }

        fun contientJaponais(texte: String): Boolean =
            texte.any { c ->
                val bloc = Character.UnicodeBlock.of(c)
                bloc == Character.UnicodeBlock.HIRAGANA ||
                    bloc == Character.UnicodeBlock.KATAKANA ||
                    bloc == Character.UnicodeBlock.CJK_UNIFIED_IDEOGRAPHS ||
                    bloc == Character.UnicodeBlock.HALFWIDTH_AND_FULLWIDTH_FORMS
            }

        private fun recoller(lignes: List<String>): String {
            val sb = StringBuilder()
            lignes.forEach { ligne ->
                if (sb.isNotEmpty()) {
                    if (sb.endsWith("-")) sb.setLength(sb.length - 1) else sb.append(' ')
                }
                sb.append(ligne)
            }
            return sb.toString()
        }

        private fun estUtile(texte: String): Boolean {
            val lettres = texte.count { it.isLetter() }
            return lettres >= 2 && texte.length >= 2
        }

        fun nomLangue(code: String): String =
            when (code) {
                TranslateLanguage.JAPANESE -> "japonais"
                TranslateLanguage.ENGLISH -> "anglais"
                TranslateLanguage.FRENCH -> "français"
                TranslateLanguage.SPANISH -> "espagnol"
                TranslateLanguage.GERMAN -> "allemand"
                TranslateLanguage.ITALIAN -> "italien"
                TranslateLanguage.CHINESE -> "chinois"
                TranslateLanguage.KOREAN -> "coréen"
                TranslateLanguage.PORTUGUESE -> "portugais"
                else -> code
            }
    }
}
