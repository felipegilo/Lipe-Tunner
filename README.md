# Simple Tuner (VST3 + Standalone)

## Como compilar
1. Instale: CMake 3.22+, Git e um compilador (Visual Studio 2022 no Windows / Xcode no Mac / GCC no Linux).
2. Dentro desta pasta:
   git clone --depth 1 https://github.com/juce-framework/JUCE.git
3. Compile:
   cmake -B build
   cmake --build build --config Release
4. O VST3 e copiado automaticamente para a pasta de plugins do sistema
   (Windows: C:\Program Files\Common Files\VST3). O app Standalone fica em build/SimpleTuner_artefacts/Release/Standalone.

## Uso
- Insira o plugin na faixa da guitarra/violao/baixo. O audio passa sem alteracao.
- A4: diapasao de referencia (440 Hz padrao).
- Gate: nivel minimo para analisar (suba se aparecerem notas com ruido).
