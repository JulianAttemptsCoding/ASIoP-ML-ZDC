// Compatibility wrapper: regenerates all ROOT/TBrowser objects, including slide 6.
void plot_slide6_erec_dist(const char* inputDir="data", const char* outDir="plots") {
    gROOT->LoadMacro("scripts/zdc_reco_browser.C");
    zdc_reco_browser(inputDir, outDir);
}
