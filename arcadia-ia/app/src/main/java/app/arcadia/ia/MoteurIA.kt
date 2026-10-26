package app.arcadia.ia

import android.content.Context
import android.util.Log
import com.google.ai.edge.litertlm.Backend
import com.google.ai.edge.litertlm.Content
import com.google.ai.edge.litertlm.Contents
import com.google.ai.edge.litertlm.Conversation
import com.google.ai.edge.litertlm.ConversationConfig
import com.google.ai.edge.litertlm.Engine
import com.google.ai.edge.litertlm.EngineConfig
import com.google.ai.edge.litertlm.Message
import com.google.ai.edge.litertlm.MessageCallback
import com.google.ai.edge.litertlm.SamplerConfig
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.callbackFlow
import kotlinx.coroutines.flow.flowOn
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext

/**
 * Le moteur d'IA : charge le modèle de langage en mémoire (sur la puce graphique si possible) et
 * produit les réponses mot à mot. Un seul modèle et une seule conversation à la fois.
 */
object MoteurIA {
    private const val TAG = "ArcadiaIA"

    sealed class Etat {
        object AucunModele : Etat()

        object EnVeille : Etat()

        data class Chargement(val nom: String) : Etat()

        data class Pret(val modele: ModeleIA, val gpu: Boolean, val vision: Boolean) : Etat()

        data class Erreur(val message: String) : Etat()
    }

    private val etatInterne = MutableStateFlow<Etat>(Etat.EnVeille)
    val etat: StateFlow<Etat> = etatInterne

    private val verrou = Mutex()
    private var moteur: Engine? = null
    private var conversation: Conversation? = null
    private var pret: Etat.Pret? = null

    /** Charge le modèle choisi (ne fait rien s'il est déjà prêt). */
    suspend fun preparer(context: Context): Etat.Pret? =
        verrou.withLock {
            val modele = Modeles.modeleChoisi(context.applicationContext)
            if (modele == null) {
                etatInterne.value = Etat.AucunModele
                return@withLock null
            }
            val gpu = Modeles.utiliserGpu(context)
            val deja = pret
            if (deja != null && deja.modele.fichier == modele.fichier && moteur != null) {
                return@withLock deja
            }
            fermerSansVerrou()
            etatInterne.value = Etat.Chargement(modele.nom)
            val chemin = Modeles.fichier(context, modele).absolutePath
            val resultat =
                withContext(Dispatchers.Default) {
                    val essais =
                        buildList {
                            if (gpu && modele.vision) add(Triple(true, true, "GPU + images"))
                            if (gpu) add(Triple(true, false, "GPU"))
                            if (modele.vision) add(Triple(false, true, "CPU + images"))
                            add(Triple(false, false, "CPU"))
                        }
                    var derniereErreur: Throwable? = null
                    for ((avecGpu, avecVision, nom) in essais) {
                        try {
                            val config =
                                EngineConfig(
                                    modelPath = chemin,
                                    backend = if (avecGpu) Backend.GPU() else Backend.CPU(),
                                    visionBackend = if (avecVision) Backend.GPU() else null,
                                    audioBackend = null,
                                    maxNumTokens = 4096,
                                )
                            val e = Engine(config)
                            e.initialize()
                            moteur = e
                            Log.i(TAG, "Modèle ${modele.nom} chargé ($nom)")
                            return@withContext Etat.Pret(modele, avecGpu, avecVision)
                        } catch (t: Throwable) {
                            Log.w(TAG, "Échec du chargement ($nom)", t)
                            derniereErreur = t
                        }
                    }
                    Etat.Erreur(
                        "Impossible de charger le modèle : ${derniereErreur?.message ?: "erreur inconnue"}. " +
                            "Le fichier est peut-être incomplet : supprimez-le puis téléchargez-le à nouveau.",
                    )
                }
            etatInterne.value = resultat
            pret = resultat as? Etat.Pret
            pret
        }

    /** Démarre une nouvelle conversation (l'ancienne est oubliée). */
    suspend fun nouvelleConversation(
        context: Context,
        instructions: String,
    ): Conversation? {
        val p = preparer(context) ?: return null
        return verrou.withLock {
            runCatching { conversation?.close() }
            conversation = null
            val e = moteur ?: return@withLock null
            val config =
                ConversationConfig(
                    systemInstruction = Contents.of(instructions),
                    samplerConfig =
                        SamplerConfig(
                            topK = p.modele.topK,
                            topP = p.modele.topP,
                            temperature = p.modele.temperature,
                        ),
                )
            runCatching { e.createConversation(config) }
                .onFailure { Log.e(TAG, "Conversation impossible", it) }
                .getOrNull()
                .also { conversation = it }
        }
    }

    /** Envoie un message et reçoit la réponse morceau par morceau. */
    fun repondre(
        conv: Conversation,
        texte: String,
        image: ByteArray? = null,
    ): Flow<String> =
        callbackFlow {
            val contenus = mutableListOf<Content>()
            if (image != null) contenus += Content.ImageBytes(image)
            contenus += Content.Text(texte)
            val rappel =
                object : MessageCallback {
                    override fun onMessage(message: Message) {
                        trySend(message.toString())
                    }

                    override fun onDone() {
                        close()
                    }

                    override fun onError(throwable: Throwable) {
                        if (throwable is java.util.concurrent.CancellationException) close() else close(throwable)
                    }
                }
            try {
                conv.sendMessageAsync(Contents.of(contenus), rappel, emptyMap())
            } catch (t: Throwable) {
                close(t)
            }
            awaitClose { }
        }.flowOn(Dispatchers.Default)

    fun interrompre() {
        runCatching { conversation?.cancelProcess() }
    }

    /** Libère la mémoire (le modèle sera rechargé à la prochaine question). */
    suspend fun fermer() = verrou.withLock { fermerSansVerrou() }

    private fun fermerSansVerrou() {
        runCatching { conversation?.close() }
        conversation = null
        runCatching { moteur?.close() }
        moteur = null
        pret = null
        etatInterne.value = Etat.EnVeille
    }
}
