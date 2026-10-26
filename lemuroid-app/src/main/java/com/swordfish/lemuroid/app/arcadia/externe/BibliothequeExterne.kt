package com.swordfish.lemuroid.app.arcadia.externe

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.DocumentsContract
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import org.json.JSONArray
import timber.log.Timber
import java.io.File
import java.util.Locale

/**
 * Les jeux des consoles externes : un dossier par console (choisi une fois par l'utilisateur),
 * analysé pour trouver les jeux, puis mémorisé dans un petit fichier pour un affichage immédiat.
 */
class BibliothequeExterne private constructor(private val context: Context) {
    private val prefs = context.getSharedPreferences("arcadia_externe", Context.MODE_PRIVATE)
    private val fichierCache = File(context.filesDir, "arcadia_jeux_externes.json")
    private val verrou = Mutex()
    private val jeuxFlow = MutableStateFlow<List<JeuExterne>>(emptyList())
    private var charge = false

    /** Tous les jeux connus, mis à jour après chaque analyse. */
    fun jeux(): StateFlow<List<JeuExterne>> = jeuxFlow

    fun dossier(console: ConsoleExterne): Uri? = prefs.getString("dossier_${console.id}", null)?.let(Uri::parse)

    fun nomDossier(console: ConsoleExterne): String? {
        val uri = dossier(console) ?: return null
        return runCatching {
            DocumentsContract.getTreeDocumentId(uri).substringAfterLast(':').ifEmpty { "/" }
        }.getOrNull()
    }

    fun definirDossier(
        console: ConsoleExterne,
        uri: Uri,
    ) {
        runCatching {
            context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }.onFailure { Timber.w(it, "Arcadia : autorisation du dossier non conservée") }
        prefs.edit().putString("dossier_${console.id}", uri.toString()).apply()
    }

    fun emulateurPrefere(console: ConsoleExterne): String? = prefs.getString("emulateur_${console.id}", null)

    fun definirEmulateurPrefere(
        console: ConsoleExterne,
        paquet: String?,
    ) {
        prefs.edit().putString("emulateur_${console.id}", paquet).apply()
    }

    suspend fun charger(): List<JeuExterne> =
        withContext(Dispatchers.IO) {
            verrou.withLock {
                if (!charge) {
                    jeuxFlow.value = lireCache()
                    charge = true
                }
                jeuxFlow.value
            }
        }

    /** Relit le fichier (utile dans le processus du jeu, qui ne fait que lire). */
    suspend fun recharger(): List<JeuExterne> =
        withContext(Dispatchers.IO) {
            verrou.withLock {
                jeuxFlow.value = lireCache()
                charge = true
                jeuxFlow.value
            }
        }

    suspend fun analyser(console: ConsoleExterne): List<JeuExterne> =
        withContext(Dispatchers.IO) {
            val arbre = dossier(console)
            val trouves = if (arbre != null) chercherJeux(console, arbre) else emptyList()
            verrou.withLock {
                val anciensListe = if (charge) jeuxFlow.value else lireCache()
                val anciens = anciensListe.filter { it.consoleId == console.id }.associateBy { it.uri }
                val fusionnes =
                    trouves.map { nouveau ->
                        val ancien = anciens[nouveau.uri]
                        if (ancien != null) {
                            nouveau.copy(dernierePartie = ancien.dernierePartie, favori = ancien.favori)
                        } else {
                            nouveau
                        }
                    }
                val autres = anciensListe.filter { it.consoleId != console.id }
                val tous = trier(autres + fusionnes)
                ecrireCache(tous)
                jeuxFlow.value = tous
                charge = true
                fusionnes
            }
        }

    suspend fun analyserTout() {
        ConsoleExterne.values().filter { dossier(it) != null }.forEach { analyser(it) }
    }

    suspend fun noterPartie(jeu: JeuExterne) = modifier(jeu) { it.copy(dernierePartie = System.currentTimeMillis()) }

    suspend fun definirFavori(
        jeu: JeuExterne,
        favori: Boolean,
    ) = modifier(jeu) { it.copy(favori = favori) }

    private suspend fun modifier(
        jeu: JeuExterne,
        changement: (JeuExterne) -> JeuExterne,
    ) = withContext(Dispatchers.IO) {
        verrou.withLock {
            val actuels = if (charge) jeuxFlow.value else lireCache()
            val nouveaux = actuels.map { if (it.uri == jeu.uri && it.consoleId == jeu.consoleId) changement(it) else it }
            ecrireCache(nouveaux)
            jeuxFlow.value = nouveaux
            charge = true
        }
    }

    private fun chercherJeux(
        console: ConsoleExterne,
        arbre: Uri,
    ): List<JeuExterne> {
        val fichiers = mutableListOf<Fichier>()
        runCatching {
            parcourir(arbre, DocumentsContract.getTreeDocumentId(arbre), 0, fichiers)
        }.onFailure { Timber.e(it, "Arcadia : impossible de lire le dossier ${console.nom}") }

        return fichiers
            .filter { it.extension in console.extensions }
            .filterNot { fichier -> estDisqueNintendoAilleurs(console, fichier) }
            .mapNotNull { fichier -> creerJeu(console, fichier) }
    }

    /** Un .iso GameCube/Wii rangé dans le dossier d'une autre console (même extension) est ignoré. */
    private fun estDisqueNintendoAilleurs(
        console: ConsoleExterne,
        fichier: Fichier,
    ): Boolean {
        if (console == ConsoleExterne.GAMECUBE_WII || fichier.extension != "iso") return false
        val plateforme = EnTeteDisque.lire(context, fichier.uri, fichier.extension)?.plateforme
        return plateforme == EnTeteDisque.Plateforme.GAMECUBE || plateforme == EnTeteDisque.Plateforme.WII
    }

    private fun creerJeu(
        console: ConsoleExterne,
        fichier: Fichier,
    ): JeuExterne? {
        val titreFichier = JeuExterne.titreDepuisFichier(fichier.nom)
        if (console != ConsoleExterne.GAMECUBE_WII) {
            return JeuExterne(
                consoleId = console.id,
                uri = fichier.uri.toString(),
                nomFichier = fichier.nom,
                titre = titreFichier,
                etiquette = console.nom,
                taille = fichier.taille,
            )
        }

        val infos = EnTeteDisque.lire(context, fichier.uri, fichier.extension)
        val nomGenerique =
            titreFichier.matches(Regex("[A-Z0-9]{6}")) ||
                titreFichier.lowercase(Locale.ROOT) in setOf("game", "jeu", "disc", "disque", "main", "boot")
        val titre =
            when {
                infos?.titre != null && nomGenerique -> infos.titre
                else -> titreFichier
            }
        val etiquette =
            when (infos?.plateforme) {
                EnTeteDisque.Plateforme.GAMECUBE -> "GameCube"
                EnTeteDisque.Plateforme.WII -> "Wii"
                else -> console.nom
            }
        return JeuExterne(
            consoleId = console.id,
            uri = fichier.uri.toString(),
            nomFichier = fichier.nom,
            titre = titre,
            etiquette = etiquette,
            code = infos?.code,
            jaquette = EnTeteDisque.urlJaquette(infos?.code),
            taille = fichier.taille,
        )
    }

    private data class Fichier(val uri: Uri, val nom: String, val taille: Long) {
        val extension: String
            get() {
                val bas = nom.lowercase(Locale.ROOT)
                return bas.substringAfterLast('.', "")
            }
    }

    private fun parcourir(
        arbre: Uri,
        idDocument: String,
        profondeur: Int,
        resultats: MutableList<Fichier>,
    ) {
        val enfants = DocumentsContract.buildChildDocumentsUriUsingTree(arbre, idDocument)
        val colonnes =
            arrayOf(
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE,
                DocumentsContract.Document.COLUMN_SIZE,
            )
        val sousDossiers = mutableListOf<String>()
        context.contentResolver.query(enfants, colonnes, null, null, null)?.use { c ->
            while (c.moveToNext()) {
                val id = c.getString(0) ?: continue
                val nom = c.getString(1) ?: continue
                val type = c.getString(2)
                val taille = if (c.isNull(3)) 0L else c.getLong(3)
                if (nom.startsWith(".")) continue
                if (type == DocumentsContract.Document.MIME_TYPE_DIR) {
                    sousDossiers += id
                } else {
                    resultats += Fichier(DocumentsContract.buildDocumentUriUsingTree(arbre, id), nom, taille)
                }
            }
        }
        if (profondeur < 4) {
            sousDossiers.forEach { parcourir(arbre, it, profondeur + 1, resultats) }
        }
    }

    private fun trier(jeux: List<JeuExterne>): List<JeuExterne> =
        jeux.sortedWith(compareBy({ it.consoleId }, { it.titre.lowercase(Locale.ROOT) }))

    private fun lireCache(): List<JeuExterne> =
        runCatching {
            if (!fichierCache.exists()) return emptyList()
            val tableau = JSONArray(fichierCache.readText())
            (0 until tableau.length()).mapNotNull { JeuExterne.depuisJson(tableau.getJSONObject(it)) }
        }.getOrElse {
            Timber.e(it, "Arcadia : cache des jeux externes illisible")
            emptyList()
        }

    private fun ecrireCache(jeux: List<JeuExterne>) {
        runCatching {
            val tableau = JSONArray()
            jeux.forEach { tableau.put(it.versJson()) }
            val temporaire = File(fichierCache.parentFile, fichierCache.name + ".tmp")
            temporaire.writeText(tableau.toString())
            if (!temporaire.renameTo(fichierCache)) {
                fichierCache.writeText(tableau.toString())
                temporaire.delete()
            }
        }.onFailure { Timber.e(it, "Arcadia : écriture du cache impossible") }
    }

    companion object {
        @Volatile
        private var instance: BibliothequeExterne? = null

        fun obtenir(context: Context): BibliothequeExterne =
            instance ?: synchronized(this) {
                instance ?: BibliothequeExterne(context.applicationContext).also { instance = it }
            }
    }
}
