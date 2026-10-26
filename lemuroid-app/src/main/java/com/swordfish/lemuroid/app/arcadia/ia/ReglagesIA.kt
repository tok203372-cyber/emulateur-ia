package com.swordfish.lemuroid.app.arcadia.ia

import android.content.Context
import com.google.mlkit.nl.translate.TranslateLanguage
import java.util.Locale

/** Réglages de l'IA locale, partagés par l'écran « IA » et le jeu. */
object ReglagesIA {
    private const val FICHIER = "arcadia_ia"

    data class Langue(val code: String, val nom: String)

    /** Langues proposées comme langue d'arrivée (les plus utiles pour les jeux). */
    val languesCibles =
        listOf(
            Langue(TranslateLanguage.FRENCH, "Français"),
            Langue(TranslateLanguage.ENGLISH, "Anglais"),
            Langue(TranslateLanguage.SPANISH, "Espagnol"),
            Langue(TranslateLanguage.GERMAN, "Allemand"),
            Langue(TranslateLanguage.ITALIAN, "Italien"),
            Langue(TranslateLanguage.PORTUGUESE, "Portugais"),
        )

    /** Langues de jeux qu'on peut préparer à l'avance pour la traduction hors ligne. */
    val languesSources =
        listOf(
            Langue(TranslateLanguage.JAPANESE, "Japonais"),
            Langue(TranslateLanguage.ENGLISH, "Anglais"),
            Langue(TranslateLanguage.CHINESE, "Chinois"),
            Langue(TranslateLanguage.KOREAN, "Coréen"),
            Langue(TranslateLanguage.SPANISH, "Espagnol"),
            Langue(TranslateLanguage.GERMAN, "Allemand"),
        )

    private fun prefs(context: Context) = context.applicationContext.getSharedPreferences(FICHIER, Context.MODE_PRIVATE)

    fun langueCible(context: Context): String {
        val defaut = TranslateLanguage.fromLanguageTag(Locale.getDefault().language) ?: TranslateLanguage.FRENCH
        return prefs(context).getString("langue_cible", null) ?: defaut
    }

    fun definirLangueCible(
        context: Context,
        code: String,
    ) = prefs(context).edit().putString("langue_cible", code).apply()

    /** Intervalle entre deux analyses de l'écran en traduction en direct (millisecondes). */
    fun intervalle(context: Context): Long = prefs(context).getLong("intervalle", 900L)

    fun definirIntervalle(
        context: Context,
        ms: Long,
    ) = prefs(context).edit().putLong("intervalle", ms).apply()

    /** Lire aussi à voix haute chaque nouveau texte traduit en direct. */
    fun lectureAuto(context: Context): Boolean = prefs(context).getBoolean("lecture_auto", false)

    fun definirLectureAuto(
        context: Context,
        actif: Boolean,
    ) = prefs(context).edit().putBoolean("lecture_auto", actif).apply()
}
