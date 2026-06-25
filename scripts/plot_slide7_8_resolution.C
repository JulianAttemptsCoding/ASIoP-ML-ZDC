// Compatibility wrapper: regenerates all ROOT/TBrowser objects, including slides 7 and 8.
void plot_slide7_8_resolution(const char* inputDir="data", const char* outDir="plots") {
    gROOT->LoadMacro("scripts/zdc_reco_browser.C");
    zdc_reco_browser(inputDir, outDir);
}
