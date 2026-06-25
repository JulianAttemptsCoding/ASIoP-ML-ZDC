// Compatibility wrapper: regenerates all ROOT/TBrowser objects, including slide 5.
void plot_slide5_energy_dump(const char* inputDir="data", const char* outDir="plots") {
    gROOT->LoadMacro("scripts/zdc_reco_browser.C");
    zdc_reco_browser(inputDir, outDir);
}
