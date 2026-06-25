void inspect_root_out(const char* path="plots/energy_reconstruction_browser.root") {
    gROOT->LoadMacro("scripts/inspect_output_root.C");
    inspect_output_root(path);
}
