// Legacy inspection filename retained intentionally.
// The revised repo no longer depends on old hardcoded /mnt/c/... paths.
// Use inspect_raw.C for raw-file inspection, inspect_output_root.C for generated output,
// or open plots/energy_reconstruction_browser.root with ROOT TBrowser.
void legacy_inspection_notice() {
    printf("Use scripts/inspect_raw.C, scripts/inspect_output_root.C, or ROOT TBrowser.\n");
}
