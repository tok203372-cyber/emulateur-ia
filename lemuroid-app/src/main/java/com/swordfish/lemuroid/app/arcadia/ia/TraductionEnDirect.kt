package com.swordfish.lemuroid.app.arcadia.ia

import android.content.Context
import android.graphics.Bitmap
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import com.swordfish.libretrodroid.GLRetroView
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import timber.log.Timber

/**
 * Traduction en direct : toutes les secondes environ, l'image du jeu est lue par l'IA et les
 * textes sont remplacés à l'écran par leur traduction. Tout se passe sur le téléphone.
 */
object TraductionEnDirect {
    data class Etat(
        val active: Boolean = false,
        val bulles: List<Bulle> = emptyList(),
        val message: String? = null,
    )

    private val etatInterne = MutableStateFlow(Etat())
    val etat: StateFlow<Etat> = etatInterne

    private var tache: Job? = null
    private var lecteur: LecteurEcran? = null
    private var voix: Voix? = null
    private var effacement: Job? = null

    val estActive: Boolean
        get() = tache?.isActive == true

    private fun lecteur(context: Context): LecteurEcran = lecteur ?: LecteurEcran(context).also { lecteur = it }

    fun voix(context: Context): Voix = voix ?: Voix(context).also { voix = it }

    fun demarrer(
        proprietaire: LifecycleOwner,
        context: Context,
        vue: () -> GLRetroView?,
    ) {
        if (estActive) return
        etatInterne.value = Etat(active = true, message = "Traduction en direct activée")
        tache =
            proprietaire.lifecycleScope.launch {
                var precedente: IntArray? = null
                var dernierTexteLu = ""
                var premierPassage = true
                while (isActive) {
                    if (!proprietaire.lifecycle.currentState.isAtLeast(Lifecycle.State.RESUMED)) {
                        delay(400)
                        continue
                    }
                    val capture = vue()?.let { CaptureJeu.capturer(it, 1280) }
                    if (capture == null) {
                        delay(500)
                        continue
                    }
                    try {
                        val empreinte = CaptureJeu.empreinte(capture.image)
                        val avant = precedente
                        if (avant == null || CaptureJeu.difference(avant, empreinte) > 3) {
                            precedente = empreinte
                            val l = lecteur(context)
                            l.langueCible = ReglagesIA.langueCible(context)
                            val blocs = l.lire(capture.image)
                            val traduits =
                                l.traduire(blocs) { langue ->
                                    etatInterne.value =
                                        etatInterne.value.copy(
                                            message = "Préparation du traducteur ($langue), une seule fois…",
                                        )
                                }
                            val bulles = CaptureJeu.versBulles(capture, traduits)
                            etatInterne.value =
                                Etat(
                                    active = true,
                                    bulles = bulles,
                                    message = if (premierPassage && bulles.isEmpty()) "Aucun texte à traduire pour l'instant" else null,
                                )
                            premierPassage = false
                            if (ReglagesIA.lectureAuto(context) && bulles.isNotEmpty()) {
                                val texte = bulles.joinToString(". ") { it.texte }
                                if (texte != dernierTexteLu) {
                                    dernierTexteLu = texte
                                    voix(context).parler(texte)
                                }
                            }
                        }
                    } catch (e: CancellationException) {
                        throw e
                    } catch (e: Exception) {
                        Timber.e(e, "Arcadia : erreur de traduction en direct")
                        etatInterne.value = etatInterne.value.copy(message = "Traduction indisponible : ${e.message}")
                    } finally {
                        capture.image.recycle()
                    }
                    delay(ReglagesIA.intervalle(context))
                }
            }
    }

    fun arreter() {
        tache?.cancel()
        tache = null
        etatInterne.value = Etat()
    }

    fun fermer() {
        arreter()
        effacement?.cancel()
        lecteur?.fermer()
        lecteur = null
        voix?.fermer()
        voix = null
    }

    /**
     * Lit l'écran une fois : affiche les traductions quelques secondes et les dit à voix haute.
     * Le texte déjà dans la bonne langue est lu tel quel.
     */
    suspend fun lireUneFois(
        proprietaire: LifecycleOwner,
        context: Context,
        vue: GLRetroView?,
    ): String? {
        val capture = vue?.let { CaptureJeu.capturer(it, 1600) } ?: return null
        return try {
            val l = lecteur(context)
            l.langueCible = ReglagesIA.langueCible(context)
            val blocs = l.lire(capture.image)
            if (blocs.isEmpty()) {
                montrerMessage(proprietaire, "Je ne vois pas de texte à l'écran")
                return null
            }
            val traduits =
                l.traduire(blocs) { langue ->
                    etatInterne.value = etatInterne.value.copy(message = "Préparation du traducteur ($langue)…")
                }
            val ordonnes = traduits.sortedWith(compareBy({ it.bloc.zone.top / 24 }, { it.bloc.zone.left }))
            val texte = ordonnes.joinToString(". ") { it.traduction ?: it.bloc.texte }
            if (!estActive) {
                etatInterne.value = Etat(active = false, bulles = CaptureJeu.versBulles(capture, ordonnes, true))
                effacerPlusTard(proprietaire, 9000)
            }
            voix(context).parler(texte)
            texte
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            Timber.e(e, "Arcadia : lecture de l'écran impossible")
            montrerMessage(proprietaire, "Lecture impossible : ${e.message}")
            null
        } finally {
            capture.image.recycle()
        }
    }

    /** Le texte visible sur une image du jeu (sans traduction), pour donner du contexte à l'assistant. */
    suspend fun texteVisible(
        context: Context,
        image: Bitmap,
    ): String? =
        try {
            lecteur(context).lire(image)
                .sortedWith(compareBy({ it.zone.top / 24 }, { it.zone.left }))
                .joinToString("\n") { it.texte }
                .takeIf { it.isNotBlank() }
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            Timber.w(e, "Arcadia : lecture du texte impossible")
            null
        }

    fun montrerMessage(
        proprietaire: LifecycleOwner,
        message: String,
    ) {
        etatInterne.value = etatInterne.value.copy(message = message)
        effacerPlusTard(proprietaire, 3500)
    }

    private fun effacerPlusTard(
        proprietaire: LifecycleOwner,
        delaiMs: Long,
    ) {
        effacement?.cancel()
        effacement =
            proprietaire.lifecycleScope.launch {
                delay(delaiMs)
                etatInterne.value =
                    if (estActive) etatInterne.value.copy(message = null) else Etat()
            }
    }
}
