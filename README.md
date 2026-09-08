# ChronoOrbit — Algorithmic Multi-Track MIDI Rhythm Generator (VST3 / CLAP / Standalone)

**ChronoOrbit** è un generatore algoritmico polimetrico e poliritmico di pattern MIDI in tempo reale sviluppato in **C++20** con il framework **JUCE 7**.

Progettato con standard audio high-performance: zero allocazioni di memoria nel blocco audio, zero jitter tramite sincronizzazione analitica ad alta precisione con `juce::AudioPlayHead`, gestione attiva lock-free delle code di Note-Off per prevenire note bloccate (hanging notes) ed emissione diretta di eventi su bus MIDI in uscita per essere registrati o indirizzati a qualsiasi sintetizzatore/drum machine in DAW.

---

## Caratteristiche Principali

### 1. Zero-Jitter DAW Transport Synchronization
- Calcolo analitico sample-accurate basato sulla posizione PPQ (Quarter Notes) fornita da `juce::AudioPlayHead`.
- Nessun drift temporale con cambi di tempo (tempo automation), signature metriche complesse o looping DAW.
- Risoluzione dei boundary di step convertiti direttamente in offset sample all'interno di ciascun `processBlock`.

### 2. Multi-Track Generative Engine (8 Lane Indipendenti)
Ogni traccia opera con parametri completamente indipendenti:
- **Polimetria**: Lunghezza di loop arbitraria per ogni traccia (es. Track 1 a 16 step, Track 2 a 7 step, Track 3 a 11 step, Track 4 a 14 step).
- **Poliritmia**: Moltiplicatori (`clock_mult`) e divisori (`clock_div`) del clock per traccia (es. 5:4, 7:3 o frazioni su divisioni del beat centrale).
- **Algoritmi di Generazione**:
  - **Euclideo Classico (Bjorklund / Bresenham)** con impulsi $K$ su $N$ step e rotazione circolare di fase $R$.
  - **Catene di Markov Non-Euclidee**: 4 stati ritmici (`Rest`, `Hit`, `Ghost Note`, `Accent`) governati da una matrice di transizione dinamica modulata dalla densità.
  - **Generatore a Processo di Poisson / Bernoulli**: Distribuzione asimmetrica per burst e cluster percussivi organici.
- **Microtiming Non-Lineare**:
  - Swing asimmetrico regolabile su step dispari (stile MPC/SP-1200).
  - Humanize / Micro-jitter programmabile per lievi sfasamenti temporali organici.
- **Modulazione Aleatoria & Quantizzazione di Scala**:
  - Probabilità di trigger per singolo step (0–100%).
  - Randomizzazione controllata di Velocity e Gate Length (5% fino a 400% dello step).
  - Quantizzazione melodica su 13 scale musicali (Maggiore, Minore Naturale, Armonica, Dorica, Frigia, Lidia, Misolidia, Pentatoniche, Hirajoshi, Insen, Esatonale, Cromatica) con offset randomico scalare.
- **Motore di Mutazione Progressiva**:
  - Trigger automatico a ogni misura ($PPQ \pmod 4.0$). Modifica stocasticamente pattern e stati di transizione in base alla percentuale globale di `Mutation`.

### 3. Prevenzione Note Bloccate (Zero Hanging Notes)
- Tabella attiva `ActiveNote` che registra channel, note number e sample globale di release.
- Invio istantaneo di noteOff e messaggi MIDI CC 123 (All Notes Off) e CC 120 (All Sound Off) in caso di:
  - Stop del trasporto DAW
  - Salto/seek indietro nella timeline o ciclo loop
  - Cambio al volo della lunghezza di step o del pattern
  - Re-triggering della stessa nota su stesso canale prima della fine del gate

### 4. Editing Interattivo dei Pattern (Doppio Click & Step Strip)
- **Doppio Click sui Nodi dell'Orbita**: Facendo doppio click su qualsiasi punto (spento o acceso) della matrice circolare, lo step inverte il suo stato e la traccia passa istantaneamente alla modalità **Custom Pattern**.
- **Striscia Step-Sequencer Lineare**: Sotto i controlli di traccia è presente una striscia di pad numerati ($1 \dots N$). Un singolo click su un pad attiva o disattiva il trigger al volo.
- **Indicatore Real-Time del Playhead**: Durante la riproduzione, il pad corrispondente allo step in esecuzione si illumina con un bordo bianco pulsante a 60 FPS.
- **Salvataggio di Stato**: Ogni modifica manuale è serializzata nell'albero APVTS, preservando il pattern editato all'interno dei progetti DAW.

### 5. Interfaccia Vettoriale Moderna (Concentric Polymetric Orbits)
- Matrice centrale con 8 orbite vettoriali concentriche renderizzate a 60 FPS in modalità lock-free.
- Nodi di step illuminati dinamicamente (hit attivi vs riposi).
- Raggio playhead radiale continuo per ogni traccia con flash d'impatto ad emissione decadente.
- Selezione interattiva delle tracce cliccando direttamente sull'orbita o sui pulsanti `T1`–`T8`.
- Controlli rotativi vettoriali custom con `OrbitLookAndFeel`.

---

## Struttura del Progetto

```
_VST_RYTHM_GEN/
├── CMakeLists.txt                # Configurazione di build moderna JUCE / C++20
├── README.md                     # Documentazione tecnica e guida DAW
├── Source/
│   ├── RhythmEngine.h            # Modelli matematici (Bjorklund, Markov, Poisson, Scale)
│   ├── RhythmEngine.cpp          # Calcolo sample-accurate, generatori e microtiming
│   ├── PluginProcessor.h         # AudioProcessor MIDI puro, APVTS e gestione code
│   ├── PluginProcessor.cpp       # DAW sync, MIDI scheduling e noteOff protection
│   ├── PluginEditor.h            # Visualizzatore orbite e interfaccia LookAndFeel
│   └── PluginEditor.cpp          # Disegno vettoriale concentrico e binding APVTS
└── build/                        # Output di compilazione
    └── ChronoOrbit_artefacts/
        └── Release/
            ├── VST3/ChronoOrbit.vst3
            └── Standalone/ChronoOrbit.app
```

---

## Compilazione

### Prerequisiti
- CMake $\ge$ 3.22
- Compilatore C++20 (Apple Clang $\ge$ 14, GCC $\ge$ 11, o MSVC 2019/2022)
- JUCE 7

### Comandi di Build
```bash
# Configurazione CMake (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Compilazione target VST3 e Standalone
cmake --build build --config Release
```

I binari generati saranno disponibili in:
- **VST3**: `build/ChronoOrbit_artefacts/Release/VST3/ChronoOrbit.vst3`
- **Standalone**: `build/ChronoOrbit_artefacts/Release/Standalone/ChronoOrbit.app`

Per installare il plugin nel percorso standard di sistema su macOS:
```bash
cp -R build/ChronoOrbit_artefacts/Release/VST3/ChronoOrbit.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

---

## Routing & Registrazione MIDI in DAW

### Ableton Live
1. Inserisci `ChronoOrbit` su una traccia MIDI (es. Track 1).
2. Crea una traccia Instrument (es. Track 2) con il tuo synth o Drum Rack preferito.
3. Nella sezione **MIDI From** di Track 2, seleziona `1-ChronoOrbit`.
4. Nel secondo menu a tendina, seleziona il canale desiderato (o `ChronoOrbit / All Channels`).
5. Imposta il Monitor su **In** oppure arma la Track 2 per la registrazione.
6. Premi Play/Record: gli eventi generati da ChronoOrbit verranno registrati in tempo reale in una clip MIDI!

### Logic Pro
- Carica `ChronoOrbit` come plugin nello slot **MIDI FX** posto in cima allo strip della traccia Instrument.

### Bitwig Studio
- Inserisci `ChronoOrbit` nella traccia prima di qualsiasi strumento virtuale. I dati MIDI fluiscono direttamente nel sintetizzatore a valle.

### Reaper
- Inserisci `ChronoOrbit` come effetto FX sulla traccia MIDI, quindi usa il routing di traccia (Sends) inviando l'output MIDI a una qualsiasi altra traccia strumento.
