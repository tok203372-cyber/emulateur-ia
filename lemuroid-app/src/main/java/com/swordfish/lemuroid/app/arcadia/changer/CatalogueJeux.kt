package com.swordfish.lemuroid.app.arcadia.changer

import android.content.Context
import com.swordfish.lemuroid.app.arcadia.ProchainJeu
import com.swordfish.lemuroid.app.arcadia.externe.BibliothequeExterne
import com.swordfish.lemuroid.app.arcadia.externe.ConsoleExterne
import com.swordfish.lemuroid.app.arcadia.externe.JeuExterne
import com.swordfish.lemuroid.app.arcadia.ui.normaliser
import com.swordfish.lemuroid.lib.library.GameSystem
import com.swordfish.lemuroid.lib.library.db.RetrogradeDatabase
import com.swordfish.lemuroid.lib.library.db.entity.Game

/** Un jeu de la bibliothèque, qu'il soit joué dans Arcadia ou dans un émulateur externe. */
data class ElementJeu(
    val cle: String,
    val titre: String,
    val console: String,
    val groupe: String,
    val interne: Game? = null,
    val externe: JeuExterne? = null,
    val dernierePartie: Long? = null,
    val favori: Boolean = false,
) {
    val titreNormalise: String = normaliser(titre)

    fun versProchain(): ProchainJeu =
        if (interne != null) {
            ProchainJeu.Interne(interne.id, interne.displayName())
        } else {
            ProchainJeu.Externe(externe!!)
        }
}

data class GroupeConsole(val id: String, val nom: String, val nombre: Int)

/** Rassemble tous les jeux (internes et externes) et permet de les retrouver par leur nom. */
object CatalogueJeux {
    suspend fun charger(
        context: Context,
        base: RetrogradeDatabase,
        exclureIdInterne: Int? = null,
    ): List<ElementJeu> {
        val internes =
            runCatching { base.gameDao().asyncSelectAll(10000) }
                .getOrDefault(emptyList())
                .filter { it.id != exclureIdInterne }
                .map { jeu ->
                    val nomConsole =
                        runCatching { context.getString(GameSystem.findById(jeu.systemId).titleResId) }
                            .getOrDefault(jeu.systemId)
                    ElementJeu(
                        cle = "i:${jeu.id}",
                        titre = jeu.displayName(),
                        console = nomConsole,
                        groupe = "i:${jeu.systemId}",
                        interne = jeu,
                        dernierePartie = jeu.lastPlayedAt,
                        favori = jeu.isFavorite,
                    )
                }

        val externes =
            runCatching { BibliothequeExterne.obtenir(context).recharger() }
                .getOrDefault(emptyList())
                .map { jeu ->
                    ElementJeu(
                        cle = "e:${jeu.uri}",
                        titre = jeu.titre,
                        console = jeu.etiquette,
                        groupe = "e:${jeu.consoleId}",
                        externe = jeu,
                        dernierePartie = jeu.dernierePartie,
                        favori = jeu.favori,
                    )
                }

        return (internes + externes).sortedBy { it.titreNormalise }
    }

    fun groupes(elements: List<ElementJeu>): List<GroupeConsole> =
        elements
            .groupBy { it.groupe }
            .map { (id, jeux) ->
                val nom =
                    if (id.startsWith("e:")) {
                        ConsoleExterne.parId(id.removePrefix("e:"))?.nom ?: jeux.first().console
                    } else {
                        jeux.first().console
                    }
                GroupeConsole(id, nom, jeux.size)
            }
            .sortedBy { normaliser(it.nom) }

    fun rechercher(
        elements: List<ElementJeu>,
        texte: String,
    ): List<ElementJeu> {
        val requete = normaliser(texte)
        if (requete.isEmpty()) return elements
        val mots = requete.split(' ').filter { it.isNotEmpty() }
        return elements
            .map { it to score(it.titreNormalise, requete, mots) }
            .filter { it.second > 0 }
            .sortedByDescending { it.second }
            .map { it.first }
    }

    /** Le jeu le plus proche d'un nom prononcé à voix haute (« change de jeu pour Zelda »). */
    fun meilleur(
        elements: List<ElementJeu>,
        texte: String,
    ): ElementJeu? {
        val requete = normaliser(texte)
        val mots = requete.split(' ').filter { it.length > 1 }
        if (mots.isEmpty()) return null
        return elements
            .map { it to score(it.titreNormalise, requete, mots) }
            .filter { it.second >= 40 }
            .maxByOrNull { it.second }
            ?.first
    }

    private fun score(
        titre: String,
        requete: String,
        mots: List<String>,
    ): Int {
        if (titre == requete) return 1000
        var total = 0
        if (titre.startsWith(requete)) total += 300
        if (titre.contains(requete)) total += 200
        val motsTitre = titre.split(' ')
        val trouves = mots.count { mot -> motsTitre.any { it.startsWith(mot) } }
        if (mots.isNotEmpty()) total += 100 * trouves / mots.size
        // Les nombres comptent beaucoup (« Mario Kart 64 » contre « Mario Kart DS »).
        val nombres = mots.filter { it.all(Char::isDigit) }
        if (nombres.isNotEmpty() && nombres.none { n -> motsTitre.contains(n) }) total -= 60
        return total
    }
}
