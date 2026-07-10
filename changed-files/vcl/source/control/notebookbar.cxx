/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>

#include <string_view>
#include <utility>
#include <cmath>

#include <vcl/builder.hxx>
#include <vcl/layout.hxx>
#include <vcl/notebookbar/notebookbar.hxx>
#include <vcl/notebookbar/NotebookBarAddonsItem.hxx>
#include <vcl/syswin.hxx>
#include <vcl/taskpanelist.hxx>
#include <vcl/NotebookbarContextControl.hxx>
#include <cppuhelper/implbase.hxx>
#include <comphelper/processfactory.hxx>
#include <rtl/bootstrap.hxx>
#include <osl/file.hxx>
#include <config_folders.h>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/frame/FrameAction.hpp>
#include <com/sun/star/ui/ContextChangeEventMultiplexer.hpp>
#include <comphelper/lok.hxx>

static OUString getCustomizedUIRootDir()
{
    OUString sShareLayer(u"${$BRAND_BASE_DIR/" LIBO_ETC_FOLDER "/" SAL_CONFIGFILE(
        "bootstrap") ":UserInstallation}/user/config/soffice.cfg/"_ustr);
    rtl::Bootstrap::expandMacros(sShareLayer);
    return sShareLayer;
}

static bool doesFileExist(std::u16string_view sUIDir, std::u16string_view sUIFile)
{
    OUString sUri = OUString::Concat(sUIDir) + sUIFile;
    osl::File file(sUri);
    return( file.open(0) == osl::FileBase::E_None );
}

/**
 * split from the main class since it needs different ref-counting mana
 */
class NotebookBarContextChangeEventListener : public ::cppu::WeakImplHelper<css::ui::XContextChangeEventListener, css::frame::XFrameActionListener>
{
    bool mbActive;
    VclPtr<NotebookBar> mpParent;
    css::uno::Reference<css::frame::XFrame> mxFrame;
public:
    NotebookBarContextChangeEventListener(NotebookBar *p, css::uno::Reference<css::frame::XFrame> xFrame) :
        mbActive(false),
        mpParent(p),
        mxFrame(std::move(xFrame))
    {}

    void setupFrameListener(bool bListen);
    void setupListener(bool bListen);

    // XContextChangeEventListener
    virtual void SAL_CALL notifyContextChangeEvent(const css::ui::ContextChangeEventObject& rEvent) override;

    // XFrameActionListener
    virtual void SAL_CALL frameAction(const css::frame::FrameActionEvent& rEvent) override;

    virtual void SAL_CALL disposing(const ::css::lang::EventObject&) override;
};

NotebookBar::NotebookBar(Window* pParent, const OUString& rID, const OUString& rUIXMLDescription,
                         const css::uno::Reference<css::frame::XFrame>& rFrame,
                         std::unique_ptr<NotebookBarAddonsItem> pNotebookBarAddonsItem)
    : Control(pParent)
    , m_pEventListener(new NotebookBarContextChangeEventListener(this, rFrame))
    , m_pViewShell(nullptr)
    , m_bIsWelded(false)
    , m_sUIXMLDescription(rUIXMLDescription)
{
    m_pEventListener->setupFrameListener(true);

    SetStyle(GetStyle() | WB_DIALOGCONTROL);
    OUString sUIDir = AllSettings::GetUIRootDir();
    bool doesCustomizedUIExist = doesFileExist(getCustomizedUIRootDir(), rUIXMLDescription);
    if ( doesCustomizedUIExist )
        sUIDir = getCustomizedUIRootDir();

    bool bIsWelded = comphelper::LibreOfficeKit::isActive();
    if (bIsWelded)
    {
        m_bIsWelded = true;
        m_xVclContentArea = VclPtr<VclVBox>::Create(this);
        m_xVclContentArea->Show();
        // now access it using GetMainContainer and set dispose callback with SetDisposeCallback
    }
    else
    {
        m_pUIBuilder.reset(
            new VclBuilder(this, sUIDir, rUIXMLDescription, rID, rFrame, true,
                           std::move(pNotebookBarAddonsItem)));

        // In the Notebookbar's .ui file must exist control handling context
        // - implementing NotebookbarContextControl interface with id "ContextContainer"
        // or "ContextContainerX" where X is a number >= 1
        NotebookbarContextControl* pContextContainer = nullptr;
        int i = 0;
        do
        {
            OUString aName = u"ContextContainer"_ustr;
            if (i)
                aName += OUString::number(i);

            pContextContainer = dynamic_cast<NotebookbarContextControl*>(m_pUIBuilder->get<Window>(aName));
            if (pContextContainer)
                m_pContextContainers.push_back(pContextContainer);
            i++;
        }
        while( pContextContainer != nullptr );
    }

    UpdateBackground();
}

void NotebookBar::SetDisposeCallback(const Link<const SfxViewShell*, void> rDisposeCallback, const SfxViewShell* pViewShell)
{
    m_rDisposeLink = rDisposeCallback;
    m_pViewShell = pViewShell;
}

NotebookBar::~NotebookBar()
{
    disposeOnce();
}

void NotebookBar::dispose()
{
    m_pContextContainers.clear();
    if (m_pSystemWindow && m_pSystemWindow->ImplIsInTaskPaneList(this))
        m_pSystemWindow->GetTaskPaneList()->RemoveWindow(this);
    m_pSystemWindow.reset();

    if (m_rDisposeLink.IsSet())
        m_rDisposeLink.Call(m_pViewShell);

    if (m_bIsWelded)
        m_xVclContentArea.disposeAndClear();
    else
        disposeBuilder();

    m_pEventListener->setupFrameListener(false);
    m_pEventListener->setupListener(false);
    m_pEventListener.clear();

    Control::dispose();
}

bool NotebookBar::PreNotify(NotifyEvent& rNEvt)
{
    // capture KeyEvents for taskpane cycling
    if (rNEvt.GetType() == NotifyEventType::KEYINPUT)
    {
        if (m_pSystemWindow)
            return m_pSystemWindow->PreNotify(rNEvt);
    }
    return Window::PreNotify( rNEvt );
}

Size NotebookBar::GetOptimalSize() const
{
    Size aSize;
    if (isLayoutEnabled(this))
        aSize = VclContainer::getLayoutRequisition(*GetWindow(GetWindowType::FirstChild));
    else
        aSize = Control::GetOptimalSize();

    // Increase the height by 20%
    aSize.setHeight(aSize.Height() * 1.20);
    return aSize;
}

void NotebookBar::setPosSizePixel(tools::Long nX, tools::Long nY, tools::Long nWidth, tools::Long nHeight, PosSizeFlags nFlags)
{
    bool bCanHandleSmallerWidth = false;
    bool bCanHandleSmallerHeight = false;

    bool bIsLayoutEnabled = isLayoutEnabled(this);
    Window *pChild = GetWindow(GetWindowType::FirstChild);

    if (bIsLayoutEnabled && pChild->GetType() == WindowType::SCROLLWINDOW)
    {
        WinBits nStyle = pChild->GetStyle();
        if (nStyle & (WB_AUTOHSCROLL | WB_HSCROLL))
            bCanHandleSmallerWidth = true;
        if (nStyle & (WB_AUTOVSCROLL | WB_VSCROLL))
            bCanHandleSmallerHeight = true;
    }

    Size aSize(GetOptimalSize());
    if (!bCanHandleSmallerWidth)
        nWidth = std::max(nWidth, aSize.Width());
    if (!bCanHandleSmallerHeight)
        nHeight = std::max(nHeight, aSize.Height());

    Control::setPosSizePixel(nX, nY, nWidth, nHeight, nFlags);

    if (bIsLayoutEnabled && (nFlags & PosSizeFlags::Size))
    {
        VclContainer::setLayoutAllocation(*pChild, Point(0, 0), Size(nWidth, nHeight));
    }
}

void NotebookBar::Resize()
{
    tools::Long nHeight = GetSizePixel().Height();

    if(m_pUIBuilder && m_pUIBuilder->get_widget_root())
    {
        vcl::Window* pWindow = m_pUIBuilder->get_widget_root()->GetChild(0);
        if (pWindow)
        {
            pWindow->setPosSizePixel(0, 0, GetSizePixel().Width(), nHeight, PosSizeFlags::All);
        }
    }
    if(m_bIsWelded)
    {
        vcl::Window* pChild = GetWindow(GetWindowType::FirstChild);
        assert(pChild);
        VclContainer::setLayoutAllocation(*pChild, Point(0, 0), Size(GetSizePixel().Width(), nHeight));
        Control::Resize();
    }
    Control::Resize();
}

void NotebookBar::Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect)
{
    // Draw the red gradient background
    Color aStartColor(0xA8, 0x12, 0x0F);
    Color aEndColor(0xC4, 0x1C, 0x18);
    Gradient aGradient;
    aGradient.SetStyle(css::awt::GradientStyle_LINEAR);
    aGradient.SetStartColor(aStartColor);
    aGradient.SetEndColor(aEndColor);
    aGradient.SetAngle(Degree10(2700)); // top-to-bottom
    
    tools::Rectangle aWinRect(Point(0, 0), GetSizePixel());
    rRenderContext.DrawGradient(aWinRect, aGradient);
    
    // Draw gold line at the bottom
    rRenderContext.SetLineColor(Color(255, 205, 0));
    rRenderContext.SetFillColor(Color(255, 205, 0));
    rRenderContext.DrawLine(Point(0, aWinRect.Bottom() - 1), Point(aWinRect.Right(), aWinRect.Bottom() - 1));
    rRenderContext.DrawLine(Point(0, aWinRect.Bottom()), Point(aWinRect.Right(), aWinRect.Bottom()));
    
    Control::Paint(rRenderContext, rRect);
}

void NotebookBar::SetSystemWindow(SystemWindow* pSystemWindow)
{
    m_pSystemWindow = pSystemWindow;
    if (!m_pSystemWindow->ImplIsInTaskPaneList(this))
        m_pSystemWindow->GetTaskPaneList()->AddWindow(this);
}

void SAL_CALL NotebookBarContextChangeEventListener::notifyContextChangeEvent(const css::ui::ContextChangeEventObject& rEvent)
{
    if (mpParent)
    {
        for (NotebookbarContextControl* pControl : mpParent->m_pContextContainers)
            pControl->SetContext(vcl::EnumContext::GetContextEnum(rEvent.ContextName));
    }
}

void NotebookBarContextChangeEventListener::setupListener(bool bListen)
{
    if (comphelper::LibreOfficeKit::isActive())
        return;

    auto xMultiplexer(css::ui::ContextChangeEventMultiplexer::get(::comphelper::getProcessComponentContext()));

    if (bListen)
    {
        try
        {
            xMultiplexer->addContextChangeEventListener(this, mxFrame->getController());
        }
        catch (const css::uno::Exception&)
        {
        }
    }
    else
        xMultiplexer->removeAllContextChangeEventListeners(this);

    mbActive = bListen;
}

void NotebookBarContextChangeEventListener::setupFrameListener(bool bListen)
{
    if (bListen)
        mxFrame->addFrameActionListener(this);
    else
        mxFrame->removeFrameActionListener(this);
}

void SAL_CALL NotebookBarContextChangeEventListener::frameAction(const css::frame::FrameActionEvent& rEvent)
{
    if (!mbActive)
        return;

    if (rEvent.Action == css::frame::FrameAction_COMPONENT_REATTACHED)
    {
        setupListener(true);
    }
    else if (rEvent.Action == css::frame::FrameAction_COMPONENT_DETACHING)
    {
        setupListener(false);
        // We don't want to give up on listening; just wait for
        // another controller to be attached to the frame.
        mbActive = true;
    }
}

void NotebookBar::SetupListener(bool bListen)
{
    m_pEventListener->setupListener(bListen);
}

void SAL_CALL NotebookBarContextChangeEventListener::disposing(const ::css::lang::EventObject&)
{
    mpParent.reset();
}

void NotebookBar::DataChanged(const DataChangedEvent& rDCEvt)
{
    UpdateBackground();
    Control::DataChanged(rDCEvt);
}

void NotebookBar::StateChanged(const  StateChangedType nStateChange )
{
    UpdateBackground();
    Control::StateChanged(nStateChange);
    Invalidate();
}

static bool lcl_IsInsideTabPage(vcl::Window* pWindow)
{
    vcl::Window* pParent = pWindow->GetParent();
    while (pParent)
    {
        if (pParent->GetType() == WindowType::TABPAGE)
            return true;
        pParent = pParent->GetParent();
    }
    return false;
}

static void lcl_ApplySettingsToChildren(vcl::Window* pWindow, const AllSettings& rSettings)
{
    pWindow->SetSettings(rSettings);
    const StyleSettings& rStyleSettings = rSettings.GetStyleSettings();
    if (pWindow->GetType() == WindowType::TABPAGE)
    {
        pWindow->SetBackground(rStyleSettings.GetDialogColor());
        pWindow->SetPaintTransparent(false);
    }
    else
    {
        pWindow->SetBackground(Wallpaper());
        pWindow->SetPaintTransparent(true);
        pWindow->EnableChildTransparentMode(true);
        pWindow->SetParentClipMode(ParentClipMode::NoClip);
    }
    
    for (vcl::Window* pChild = pWindow->GetWindow(GetWindowType::FirstChild); pChild;
         pChild = pChild->GetWindow(GetWindowType::Next))
    {
        lcl_ApplySettingsToChildren(pChild, rSettings);
    }
}

void NotebookBar::UpdateBackground()
{
    // Set solid flag red background color
    SetBackground(Color(0xDA, 0x25, 0x1D));
    UpdateDefaultSettings();
    GetOutDev()->SetSettings( DefaultSettings );

    // Apply recursively to all child windows to override default system theme settings
    lcl_ApplySettingsToChildren(this, DefaultSettings);

    Invalidate(tools::Rectangle(Point(0,0), GetSizePixel()));
}

void NotebookBar::UpdateDefaultSettings()
{
    AllSettings aAllSettings( GetSettings() );
    StyleSettings aStyleSet( aAllSettings.GetStyleSettings() );

    ::Color aTextColor(255, 244, 243);     // Cream white #FFF4F3 (inactive tab text on red drape)
    ::Color aHighlightColor(0xDA, 0x25, 0x1D);  // Brand red #DA251D (active tab text on cream tab bg)
    ::Color aRolloverColor(255, 224, 138); // Warm gold #FFE08A (hovered tab text)
    ::Color aCreamBg(255, 251, 242);       // Cream background #FFFBF2 (for active tab and tab pages)
    ::Color aDarkTextColor(0x1A, 0x14, 0x10);  // Dark ink #1A1410 (for buttons and labels on cream bg)
    ::Color aDarkLabelColor(0x3A, 0x2F, 0x26); // Dark label #3A2F26

    aStyleSet.SetDialogTextColor( aDarkTextColor );
    aStyleSet.SetButtonTextColor( aDarkTextColor );
    aStyleSet.SetRadioCheckTextColor( aDarkTextColor );
    aStyleSet.SetGroupTextColor( aDarkTextColor );
    aStyleSet.SetLabelTextColor( aDarkLabelColor );
    aStyleSet.SetWindowTextColor( aDarkTextColor );
    aStyleSet.SetTabTextColor(aTextColor);
    aStyleSet.SetToolTextColor(aDarkTextColor);
    aStyleSet.SetTabHighlightTextColor(aHighlightColor);
    aStyleSet.SetTabRolloverTextColor(aRolloverColor);
    aStyleSet.SetDialogColor(aCreamBg);

    aAllSettings.SetStyleSettings(aStyleSet);
    DefaultSettings = std::move(aAllSettings);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
