/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <sal/config.h>
#include <config_folders.h>

#include <cassert>

#include <about.hxx>

#include <osl/process.h> //osl_getProcessLocale
#include <rtl/bootstrap.hxx>
#include <sal/log.hxx> //SAL_WARN
#include <tools/color.hxx>
#include <vcl/bitmap.hxx>
#include <vcl/graph.hxx> //Graphic
#include <vcl/graphicfilter.hxx>
#include <vcl/settings.hxx> //GetSettings
#include <vcl/svapp.hxx> //Application::
#include <vcl/weld/Builder.hxx>
#include <vcl/weld/weld.hxx>
#include <unotools/resmgr.hxx> //Translate

#include <config_buildid.h> //EXTRA_BUILDID
#include <config_features.h>
#include <dialmgr.hxx> //CuiResId
#include <i18nlangtag/languagetag.hxx>
#include <sfx2/app.hxx> //SfxApplication::loadBrandSvg
#include <sfx2/dispatch.hxx>
#include <sfx2/sfxsids.hrc>
#include <osl/file.hxx>
#include <com/sun/star/system/SystemShellExecute.hpp>
#include <com/sun/star/system/SystemShellExecuteFlags.hpp>
#include <comphelper/processfactory.hxx>
#include <strings.hrc>
#include <svtools/langhelp.hxx>
#include <unotools/bootstrap.hxx> //utl::Bootstrap::getBuildIdData
#include <unotools/configmgr.hxx> //ConfigManager::
#include <vcl/unohelp2.hxx>

#include <config_feature_opencl.h>
#if HAVE_FEATURE_OPENCL
#include <opencl/openclwrapper.hxx>
#endif
#include <officecfg/Office/Calc.hxx>
#include <officecfg/Office/Common.hxx>

using namespace ::com::sun::star::uno;

AboutDialog::AboutDialog(weld::Window* pParent)
    : GenericDialogController(pParent, u"cui/ui/aboutdialog.ui"_ustr, u"AboutDialog"_ustr)
    , m_pCreditsButton(m_xBuilder->weld_link_button(u"btnCredits"_ustr))
    , m_pWebsiteButton(m_xBuilder->weld_link_button(u"btnWebsite"_ustr))
    , m_pReleaseNotesButton(m_xBuilder->weld_link_button(u"btnReleaseNotes"_ustr))
    , m_pCloseButton(m_xBuilder->weld_button(u"btnClose"_ustr))
    , m_pCopyButton(m_xBuilder->weld_button(u"btnCopyVersion"_ustr))
    , m_pBrandImage(m_xBuilder->weld_image(u"imBrand"_ustr))
    , m_pAboutImage(m_xBuilder->weld_image(u"imAbout"_ustr))
    , m_pVersionLabel(m_xBuilder->weld_label(u"lbVersionString"_ustr))
    , m_pBuildCaption(m_xBuilder->weld_label(u"lbBuild"_ustr))
    , m_pBuildLabel(m_xBuilder->weld_link_button(u"lbBuildString"_ustr))
    , m_pEnvLabel(m_xBuilder->weld_label(u"lbEnvString"_ustr))
    , m_pUILabel(m_xBuilder->weld_label(u"lbUIString"_ustr))
    , m_pLocaleLabel(m_xBuilder->weld_label(u"lbLocaleString"_ustr))
    , m_pMiscLabel(m_xBuilder->weld_label(u"lbMiscString"_ustr))
    , m_pCopyrightLabel(m_xBuilder->weld_label(u"lbCopyright"_ustr))
{
    // Labels
    m_pVersionLabel->set_label(GetVersionString());

    OUString sbuildId = GetBuildString();
    if (IsStringValidGitHash(sbuildId))
    {
        const tools::Long nMaxChar = 25;
        // Point the build link at the VN-Office source repository (published for
        // MPL 2.0 / LGPL v3+ source-disclosure compliance) rather than upstream
        // LibreOffice, so it stays valid once local changes are committed and
        // guides users to the corresponding source.
        m_pBuildLabel->set_uri(u"https://github.com/toanda2017/vn-office-source"_ustr);
        m_pBuildLabel->set_label(
            sbuildId.getLength() > nMaxChar
                ? sbuildId.replaceAt(nMaxChar, sbuildId.getLength() - nMaxChar, u"...")
                : sbuildId);
    }
    else
    {
        m_pBuildCaption->hide();
        m_pBuildLabel->hide();
    }

    m_pEnvLabel->set_label(Application::GetHWOSConfInfo(1));
    m_pUILabel->set_label(Application::GetHWOSConfInfo(2));
    m_pLocaleLabel->set_label(GetLocaleString());
    m_pMiscLabel->set_label(GetMiscString());
    m_pCopyrightLabel->set_label(GetCopyrightString());

    // Images - Load about.png from brand directory and scale down to fit dialog
    {
        OUString sAboutURL = "$BRAND_BASE_DIR/" LIBO_ETC_FOLDER "/shell/about.png";
        rtl::Bootstrap::expandMacros(sAboutURL);

        SvFileStream aStream(sAboutURL, StreamMode::READ);
        if (!aStream.GetError())
        {
            GraphicFilter& rFilter = GraphicFilter::GetGraphicFilter();
            Graphic aGraphic;
            if (rFilter.ImportGraphic(aGraphic, sAboutURL, aStream) == ERRCODE_NONE)
            {
                const Size aOrigSize = aGraphic.GetSizePixel();
                const tools::Long nTargetWidth = 480;
                if (aOrigSize.Width() > nTargetWidth && aOrigSize.Width() > 0)
                {
                    Bitmap aBmp = aGraphic.GetBitmap();
                    const Size aNewSize(nTargetWidth,
                                        aOrigSize.Height() * nTargetWidth / aOrigSize.Width());
                    aBmp.Scale(aNewSize, BmpScaleFlag::BestQuality);
                    aGraphic = Graphic(aBmp);
                }
                m_pBrandImage->set_image(aGraphic.GetXGraphic());
            }
        }
    }
    m_pAboutImage->hide();

    // Links - Credits button opens donate.png from brand directory
    {
        OUString sDonatePath = "$BRAND_BASE_DIR/" LIBO_ETC_FOLDER "/shell/donate.png";
        rtl::Bootstrap::expandMacros(sDonatePath);
        // Convert to file:// URI
        OUString sDonateURI;
        if (osl::FileBase::getFileURLFromSystemPath(sDonatePath, sDonateURI) == osl::FileBase::E_None)
            m_pCreditsButton->set_uri(sDonateURI);
        else
            m_pCreditsButton->set_uri(sDonatePath);
        m_pCreditsButton->connect_activate_link(LINK(this, AboutDialog, OnCreditsClick));
    }

    OUString sURL(officecfg::Office::Common::Help::StartCenter::InfoURL::get());
    localizeWebserviceURI(sURL);
    m_pWebsiteButton->set_uri(sURL);

    // See also SID_WHATSNEW in sfx2/source/appl/appserv.cxx
    sURL = officecfg::Office::Common::Menus::ReleaseNotesURL::get()
           + "?LOvers=" + utl::ConfigManager::getProductVersion()
           + "&LOlocale=" + LanguageTag(utl::ConfigManager::getUILocale()).getBcp47();
    m_pReleaseNotesButton->set_uri(sURL);

    // Handler
    m_pCopyButton->connect_clicked(LINK(this, AboutDialog, HandleClick));
    m_pCloseButton->grab_focus();
}

AboutDialog::~AboutDialog() {}

IMPL_STATIC_LINK(AboutDialog, OnCreditsClick, weld::LinkButton&, rButton, bool)
{
    // Open donate.png in the default image viewer via shell
    try
    {
        OUString sDonatePath = "$BRAND_BASE_DIR/" LIBO_ETC_FOLDER "/shell/donate.png";
        rtl::Bootstrap::expandMacros(sDonatePath);
        OUString sDonateURI;
        if (osl::FileBase::getFileURLFromSystemPath(sDonatePath, sDonateURI) != osl::FileBase::E_None)
            sDonateURI = rButton.get_uri();

        Reference<css::system::XSystemShellExecute> const xSystemShellExecute(
            css::system::SystemShellExecute::create(
                ::comphelper::getProcessComponentContext()));
        xSystemShellExecute->execute(sDonateURI, OUString(),
            css::system::SystemShellExecuteFlags::URIS_ONLY);
    }
    catch (const Exception&) {}
    return true; // consume the click; default URL action is suppressed
}

bool AboutDialog::IsStringValidGitHash(std::u16string_view hash)
{
    return std::all_of(hash.begin(), hash.end(),
                       [](auto& rSymbol) { return std::isxdigit(rSymbol); });
}

OUString AboutDialog::GetVersionString()
{
    OUString arch;
    auto const ok = rtl::Bootstrap::get(u"_ARCH"_ustr, arch);
    assert(ok);
    (void)ok;
    return CuiResId(TranslateId(nullptr, "%ABOUTBOXPRODUCTVERSION%ABOUTBOXPRODUCTVERSIONSUFFIX"))
           + " (" + arch + ")";
}

OUString AboutDialog::GetBuildString()
{
    OUString sBuildId(utl::Bootstrap::getBuildIdData(u""_ustr));
    SAL_WARN_IF(sBuildId.isEmpty(), "cui.dialogs", "No BUILDID in bootstrap file");

    return sBuildId;
}

OUString AboutDialog::GetLocaleString(const bool bLocalized)
{
    OUString sLocaleStr;

    rtl_Locale* pLocale;
    osl_getProcessLocale(&pLocale);
    if (pLocale && pLocale->Language)
    {
        if (pLocale->Country && rtl_uString_getLength(pLocale->Country) > 0)
            sLocaleStr = OUString::unacquired(&pLocale->Language) + "_"
                         + OUString::unacquired(&pLocale->Country);
        else
            sLocaleStr = OUString(pLocale->Language);
        if (pLocale->Variant && rtl_uString_getLength(pLocale->Variant) > 0)
            sLocaleStr += OUString(pLocale->Variant);
    }

    sLocaleStr = Application::GetSettings().GetLanguageTag().getBcp47() + " (" + sLocaleStr + ")";

    OUString aUILocaleStr = Application::GetSettings().GetUILanguageTag().getBcp47();
    OUString sUILocaleStr;
    if (bLocalized)
        sUILocaleStr = CuiResId(RID_CUISTR_ABOUT_UILOCALE);
    else
        sUILocaleStr = Translate::get(RID_CUISTR_ABOUT_UILOCALE,
                                      Translate::Create("cui", LanguageTag(u"en-US"_ustr)));

    if (sUILocaleStr.indexOf("$LOCALE") == -1)
    {
        SAL_WARN("cui.dialogs", "translated uilocale string in translations "
                                "doesn't contain $LOCALE placeholder");
        sUILocaleStr += " $LOCALE";
    }
    sUILocaleStr = sUILocaleStr.replaceAll("$LOCALE", aUILocaleStr);

    return sLocaleStr + "; " + sUILocaleStr;
}

OUString AboutDialog::GetMiscString()
{
    OUString sMisc;

    bool const extra = EXTRA_BUILDID[0] != '\0';
    // extracted from the 'if' to avoid Clang -Wunreachable-code
    if (extra)
    {
        sMisc = EXTRA_BUILDID "\n";
    }

    OUString aCalcMode; // Calc calculation mode

#if HAVE_FEATURE_OPENCL
    if (openclwrapper::GPUEnv::isOpenCLEnabled())
        aCalcMode += " CL";
#endif

    static const bool bThreadingProhibited = std::getenv("SC_NO_THREADED_CALCULATION");
    bool bThreadedCalc = officecfg::Office::Calc::Formula::Calculation::
        UseThreadedCalculationForFormulaGroups::get();

    if (!bThreadingProhibited && bThreadedCalc)
    {
        aCalcMode += " threaded";
    }

    if (officecfg::Office::Calc::Defaults::Sheet::JumboSheets::get())
    {
        aCalcMode += " Jumbo";
    }

    if (aCalcMode.isEmpty())
        aCalcMode = " default";
    sMisc += "Calc:" + aCalcMode;

    return sMisc;
}

OUString AboutDialog::GetCopyrightString()
{
    return CuiResId(RID_CUISTR_ABOUT_VENDOR);
}

// special labels to comply with previous version info
// untranslated English for QA
IMPL_LINK_NOARG(AboutDialog, HandleClick, weld::Button&, void)
{
    css::uno::Reference<css::datatransfer::clipboard::XClipboard> xClipboard
        = m_pVersionLabel->get_clipboard();

    OUString sInfo = "Version: " + m_pVersionLabel->get_label()
                     + "\n" // version
                       "Build ID: "
                     + GetBuildString() + "\n" + // build id
                     Application::GetHWOSConfInfo(0, false)
                     + "\n" // env+UI
                       "Locale: "
                     + GetLocaleString(false) + "\n" + // locale
                     GetMiscString(); // misc

    vcl::unohelper::TextDataObject::CopyStringTo(sInfo, xClipboard);
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
