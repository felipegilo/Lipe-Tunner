cmake_minimum_required(VERSION 3.22)
project(SimpleTuner VERSION 0.1.0)

# Coloque a pasta do JUCE ao lado deste arquivo (git clone https://github.com/juce-framework/JUCE.git)
add_subdirectory(JUCE)

juce_add_plugin(SimpleTuner
    COMPANY_NAME "Lipe"
    PLUGIN_MANUFACTURER_CODE Snme
    PLUGIN_CODE Tnr1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Lipe Tunner"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(SimpleTuner PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(SimpleTuner PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SimpleTuner
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

