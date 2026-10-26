package app.arcadia.ia

import android.app.DownloadManager
import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream

/** Téléchargement des modèles (avec le gestionnaire de téléchargements d'Android) et import de fichiers. */
object Telechargements {
    data class Progression(val octets: Long, val total: Long, val statut: Int, val raison: Int) {
        val fraction: Float
            get() = if (total > 0) (octets.toFloat() / total).coerceIn(0f, 1f) else 0f
        val enCours: Boolean
            get() = statut == DownloadManager.STATUS_RUNNING || statut == DownloadManager.STATUS_PENDING ||
                statut == DownloadManager.STATUS_PAUSED
    }

    private fun gestionnaire(context: Context) = context.getSystemService(DownloadManager::class.java)

    fun demarrer(
        context: Context,
        modele: ModeleIA,
    ): Long? {
        val url = modele.url ?: return null
        Modeles.fichier(context, modele).delete()
        val demande =
            DownloadManager.Request(Uri.parse(url))
                .setTitle("Arcadia IA : ${modele.nom}")
                .setDescription("Téléchargement du modèle d'intelligence artificielle")
                .setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED)
                .setDestinationInExternalFilesDir(context, "modeles", modele.fichier)
                .setAllowedOverMetered(true)
                .setAllowedOverRoaming(false)
        val numero = gestionnaire(context).enqueue(demande)
        Modeles.noterTelechargement(context, modele.id, numero)
        return numero
    }

    fun progression(
        context: Context,
        numero: Long,
    ): Progression? {
        val requete = DownloadManager.Query().setFilterById(numero)
        gestionnaire(context).query(requete)?.use { c ->
            if (!c.moveToFirst()) return null
            val octets = c.getLong(c.getColumnIndexOrThrow(DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR))
            val total = c.getLong(c.getColumnIndexOrThrow(DownloadManager.COLUMN_TOTAL_SIZE_BYTES))
            val statut = c.getInt(c.getColumnIndexOrThrow(DownloadManager.COLUMN_STATUS))
            val raison = c.getInt(c.getColumnIndexOrThrow(DownloadManager.COLUMN_REASON))
            return Progression(octets, total, statut, raison)
        }
        return null
    }

    fun annuler(
        context: Context,
        modele: ModeleIA,
    ) {
        Modeles.telechargementEnCours(context)[modele.id]?.let { gestionnaire(context).remove(it) }
        Modeles.noterTelechargement(context, modele.id, null)
        Modeles.fichier(context, modele).delete()
    }

    fun supprimer(
        context: Context,
        fichier: File,
    ) {
        fichier.delete()
    }

    /** Copie un fichier .litertlm choisi par l'utilisateur dans le dossier des modèles. */
    suspend fun importer(
        context: Context,
        uri: Uri,
        surProgression: (Float) -> Unit,
    ): Result<File> =
        withContext(Dispatchers.IO) {
            runCatching {
                var nom = "modele_importe.litertlm"
                var taille = -1L
                context.contentResolver.query(uri, null, null, null, null)?.use { c ->
                    if (c.moveToFirst()) {
                        val iNom = c.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                        val iTaille = c.getColumnIndex(OpenableColumns.SIZE)
                        if (iNom >= 0) c.getString(iNom)?.let { nom = it }
                        if (iTaille >= 0 && !c.isNull(iTaille)) taille = c.getLong(iTaille)
                    }
                }
                require(nom.endsWith(".litertlm", ignoreCase = true)) {
                    "Ce fichier n'est pas un modèle .litertlm"
                }
                val destination = File(Modeles.dossier(context), nom)
                val temporaire = File(destination.parentFile, "$nom.partiel")
                val entree = context.contentResolver.openInputStream(uri) ?: error("Fichier illisible")
                entree.use { flux ->
                    FileOutputStream(temporaire).use { sortie ->
                        val tampon = ByteArray(1 shl 20)
                        var copie = 0L
                        while (true) {
                            val n = flux.read(tampon)
                            if (n < 0) break
                            sortie.write(tampon, 0, n)
                            copie += n
                            if (taille > 0) surProgression(copie.toFloat() / taille)
                        }
                    }
                }
                destination.delete()
                check(temporaire.renameTo(destination)) { "Copie impossible" }
                destination
            }
        }
}
