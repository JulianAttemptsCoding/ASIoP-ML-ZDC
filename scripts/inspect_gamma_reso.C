// inspect_gamma_reso.C — finds gamma energy array indices and TF1 requirement functions
void inspect_gamma_reso() {
    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";

    // gamma res2 file
    TString gpath = TString::Format("%s/20260421_gamma_LYSO_diffAngle/res2_1_m0_w1_gamma.root", base);
    TFile *fg = TFile::Open(gpath);
    printf("=== gamma res2_1_m0_w1 gReso_dcb points ===\n");
    TGraphErrors *grg = (TGraphErrors*)fg->Get("gReso_dcb");
    for (int i = 0; i < grg->GetN(); i++)
        printf("  [%d] E=%.3f GeV  reso=%.4f\n", i, grg->GetX()[i], grg->GetY()[i]);

    printf("\n=== gamma hERatioIndv_dcb TLatex (energy labels from pad) ===\n");
    TCanvas *cd = (TCanvas*)fg->Get("c_Erec_dcb");
    TList *prims = cd->GetListOfPrimitives();
    TIter it(prims);
    TObject *obj;
    int padIdx = 0;
    while ((obj = it())) {
        if (obj->InheritsFrom("TPad")) {
            TPad *pad = (TPad*)obj;
            TIter it2(pad->GetListOfPrimitives());
            TObject *o2;
            bool hasTH1 = false;
            TString h1name;
            while ((o2 = it2())) {
                if (o2->InheritsFrom("TH1")) { hasTH1 = true; h1name = o2->GetName(); }
                if (TString(o2->ClassName()) == "TLatex") {
                    TLatex *lx = (TLatex*)o2;
                    printf("  pad[%d] %s  TLatex: '%s'\n", padIdx, h1name.Data(), lx->GetTitle());
                }
            }
            padIdx++;
        }
    }
    fg->Close();

    // neutron: check c_Erec_res TF1 details (requirement curves)
    TString npath = TString::Format("%s/20260324_neutron_LYSO_diffAngle/res2_1_m0_w0_neutron.root", base);
    TFile *fn = TFile::Open(npath);
    printf("\n=== neutron c_Erec_res TF1 parameters ===\n");
    TCanvas *cn = (TCanvas*)fn->Get("c_Erec_res");
    TIter itn(cn->GetListOfPrimitives());
    TObject *on;
    while ((on = itn())) {
        if (TString(on->ClassName()) == "TF1") {
            TF1 *f = (TF1*)on;
            printf("  TF1: %s  xmin=%.1f xmax=%.1f  formula=%s\n",
                   f->GetName(), f->GetXmin(), f->GetXmax(), f->GetFormula()->GetExpFormula().Data());
            printf("       params: ");
            for (int i = 0; i < f->GetNpar(); i++) printf("p%d=%.4f ", i, f->GetParameter(i));
            printf("\n");
        }
    }
    fn->Close();

    // gamma: same
    TFile *fg2 = TFile::Open(gpath);
    printf("\n=== gamma c_Erec_res TF1 parameters ===\n");
    TCanvas *cg = (TCanvas*)fg2->Get("c_Erec_res");
    TIter itg(cg->GetListOfPrimitives());
    TObject *og;
    while ((og = itg())) {
        if (TString(og->ClassName()) == "TF1") {
            TF1 *f = (TF1*)og;
            printf("  TF1: %s  xmin=%.1f xmax=%.1f  formula=%s\n",
                   f->GetName(), f->GetXmin(), f->GetXmax(), f->GetFormula()->GetExpFormula().Data());
            printf("       params: ");
            for (int i = 0; i < f->GetNpar(); i++) printf("p%d=%.4f ", i, f->GetParameter(i));
            printf("\n");
        }
    }
    fg2->Close();

    printf("\nDone.\n");
}
