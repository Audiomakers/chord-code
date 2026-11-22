cmake_minimum_required(VERSION 3.22)
project(ChordCodePro VERSION 1.4.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
 set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum supported macOS version")
endif()
option(CCP_FETCH_JUCE "Fetch JUCE when it is not installed" ON)
find_package(JUCE CONFIG QUIET)
if(NOT JUCE_FOUND AND CCP_FETCH_JUCE)
 include(FetchContent)
 FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12)
 FetchContent_MakeAvailable(JUCE)
endif()
add_library(chordcode_pro_core STATIC Source/Core/Music.cpp Source/Core/Music.h Source/Core/MidiFile.cpp Source/Core/MidiFile.h)
target_include_directories(chordcode_pro_core PUBLIC Source)
target_compile_features(chordcode_pro_core PUBLIC cxx_std_20)
set_target_properties(chordcode_pro_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(TARGET juce::juce_audio_utils)
 if(WIN32)
  if(juce_SOURCE_DIR)
   set(JUCE_SOURCE_DIR "${juce_SOURCE_DIR}")
  endif()
  if(JUCE_SOURCE_DIR AND EXISTS "${JUCE_SOURCE_DIR}/modules/juce_gui_basics/native/juce_NativeMessageBox_windows.cpp"
     AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/patch-juce-windows.cmake")
   include(scripts/patch-juce-windows.cmake)
  endif()
 endif()
 file(GLOB CCP_PIANO CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Piano/*.ogg")
 file(GLOB CCP_DRUMS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Drums/*.wav")
 juce_add_binary_data(ChordCodeProSamples SOURCES
  ${CCP_PIANO} ${CCP_DRUMS})
 set(CCP_PLUGIN_FORMATS VST3)
 if(APPLE)
  list(APPEND CCP_PLUGIN_FORMATS AU)
 elseif(WIN32)
  list(APPEND CCP_PLUGIN_FORMATS Standalone)
 endif()
 juce_add_plugin(ChordCodePro
  COMPANY_NAME "Audio Makers"
  BUNDLE_ID "com.audiomakers.chordcodepro"
  PLUGIN_MANUFACTURER_CODE AuMk
 PLUGIN_CODE CcPr
  ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Brand/chord-code-pro-icon-1024.png"
  ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Brand/chord-code-pro-icon-256.png"
  FORMATS ${CCP_PLUGIN_FORMATS}
  PRODUCT_NAME "Chord Code"
  VST3_CATEGORIES Instrument Synth
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  VST3_AUTO_MANIFEST FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)
 if(APPLE)
  set_target_properties(ChordCodePro PROPERTIES
   XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.audiomakers.chordcodepro"
   XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES)
 endif()
 target_sources(ChordCodePro PRIVATE
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/UpdateChecker.cpp Source/UpdateChecker.h
  Source/License.cpp Source/License.h
  Source/ModalPanels.cpp Source/ModalPanels.h
  Source/PreviewSynth.cpp Source/PreviewSynth.h
  Source/Synth.cpp Source/Synth.h Source/Tone.cpp Source/Tone.h Source/Sets.cpp Source/Sets.h
  Source/DrumMachine.cpp Source/DrumMachine.h
  Source/LookAndFeel.cpp Source/LookAndFeel.h
  Source/ChordCodeTheme.cpp Source/ChordCodeTheme.h
  Source/EmbeddedFonts.cpp Source/EmbeddedFonts.h Source/AMUI.cpp Source/AMUI.h
)
 target_link_libraries(ChordCodePro PRIVATE chordcode_pro_core ChordCodeProSamples juce::juce_audio_utils juce::juce_audio_processors juce::juce_audio_formats juce::juce_gui_basics PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
 target_compile_definitions(ChordCodePro PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_DIRECT2D=0 JUCE_VST3_CAN_REPLACE_VST2=0)
 if(MINGW OR (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
  target_compile_options(ChordCodePro PRIVATE -Wno-error=date-time)
  target_compile_definitions(ChordCodePro PRIVATE D2D1_SATURATION_PROP_SATURATION=0 CaretPosition_Unknown=0 CaretPosition_EndOfLine=1 CaretPosition_BeginningOfLine=2 NS_E_NO_MORE_SAMPLES=0xC00D0BCF)
   target_link_libraries(ChordCodePro PRIVATE kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 ws2_32 wininet version shlwapi winmm imm32 comctl32 dwmapi dwrite d2d1 dxgi d3d11 dcomp dxguid usp10 dbghelp)
 endif()
 if(WIN32 AND CCP_CROSSCOMPILE_NO_RC)
  target_sources(ChordCodePro_VST3 PRIVATE Source/Windows/ChordCodePro.rc)
  set_source_files_properties(Source/Windows/ChordCodePro.rc PROPERTIES LANGUAGE RC)
 endif()
 if(WIN32)
  # JUCE's RC rule depends on the icon but also needs the changed product metadata.
  get_target_property(CCP_INFO_FILE ChordCodePro JUCE_INFO_FILE)
  get_target_property(CCP_GENERATED_DIR ChordCodePro JUCE_GENERATED_SOURCES_DIRECTORY)
  add_custom_command(OUTPUT "${CCP_GENERATED_DIR}/ChordCodePro_resources.rc"
   APPEND DEPENDS "${CCP_INFO_FILE}")
  set(CCP_INSTALLER_DIR "${CMAKE_CURRENT_BINARY_DIR}/InstallerResources")
  file(MAKE_DIRECTORY "${CCP_INSTALLER_DIR}")
  file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Installer/Installer.manifest" CCP_INSTALLER_MANIFEST)
  file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Brand/chord-code-pro.ico" CCP_INSTALLER_ICON)
  file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}/ChordCodePro_artefacts/Release/VST3/Chord Code.vst3/Contents/x86_64-win/Chord Code.vst3" CCP_INSTALLER_PLUGIN)
  file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}/ChordCodePro_artefacts/Release/Standalone/Chord Code.exe" CCP_INSTALLER_APP)
  file(WRITE "${CCP_INSTALLER_DIR}/Installer.rc"
   "#define RT_MANIFEST 24\n1 RT_MANIFEST \"${CCP_INSTALLER_MANIFEST}\"\n1 ICON \"${CCP_INSTALLER_ICON}\"\n201 RCDATA \"${CCP_INSTALLER_PLUGIN}\"\n202 RCDATA \"${CCP_INSTALLER_APP}\"\n")
  add_executable(ChordCodeProInstaller WIN32 Installer/Installer.cpp "${CCP_INSTALLER_DIR}/Installer.rc")
  target_compile_definitions(ChordCodeProInstaller PRIVATE UNICODE _UNICODE "CCP_VERSION=L\"${PROJECT_VERSION}\"")
  target_link_libraries(ChordCodeProInstaller PRIVATE shell32 ole32 uuid user32 gdi32 gdiplus dwmapi advapi32)
  if(MSVC)
   # The interface text is Portuguese; read the source as UTF-8.
   target_compile_options(ChordCodeProInstaller PRIVATE /utf-8)
  endif()
  add_dependencies(ChordCodeProInstaller ChordCodePro_VST3 ChordCodePro_Standalone)
  set_target_properties(ChordCodeProInstaller PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/Release/Windows"
   LINK_FLAGS "/MANIFEST:NO")
 endif()
endif()
enable_testing()
if(TARGET ChordCodePro)
 juce_add_console_app(chordcode_audio_tests PRODUCT_NAME "Chord Code Audio Tests")
 target_sources(chordcode_audio_tests PRIVATE Tests/AudioTests.cpp)
 target_include_directories(chordcode_audio_tests PRIVATE Source $<TARGET_PROPERTY:ChordCodePro,INCLUDE_DIRECTORIES>)
 target_compile_definitions(chordcode_audio_tests PRIVATE $<TARGET_PROPERTY:ChordCodePro,COMPILE_DEFINITIONS>)
 target_link_libraries(chordcode_audio_tests PRIVATE ChordCodePro juce::juce_audio_utils)
 add_test(NAME chordcode_audio_tests COMMAND chordcode_audio_tests)
endif()
add_executable(chordcode_pro_tests Tests/CoreTests.cpp)
target_link_libraries(chordcode_pro_tests PRIVATE chordcode_pro_core)
add_test(NAME chordcode_pro_core_tests COMMAND chordcode_pro_tests)
