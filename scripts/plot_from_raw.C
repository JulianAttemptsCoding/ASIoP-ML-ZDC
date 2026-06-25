// Compatibility wrapper. Main implementation is zdc_reco_browser.C.
void plot_from_raw(const char* inputDir="data", const char* outDir="plots") {
    gROOT->LoadMacro("scripts/zdc_reco_browser.C");
    zdc_reco_browser(inputDir, outDir);
}
