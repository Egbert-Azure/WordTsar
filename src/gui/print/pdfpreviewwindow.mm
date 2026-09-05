//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2026 Egbert H. Schroeer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

/**
 * @brief Native PDFKit print-preview window, replacing QPrintPreviewDialog.
 *
 * A plain NSWindow with a PDFView and a small toolbar (zoom, page nav,
 * Print). Print goes through PDFView's own printWithInfo:autoRotate:,
 * which prints the PDFDocument already on screen directly -- there is no
 * second render pass, so preview and print can't drift apart the way the
 * old QPrinter::resolution()-dependent Qt path did.
 */

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <PDFKit/PDFKit.h>

#include "pdfpreviewwindow.h"

@interface WTPDFPreviewController : NSObject <NSWindowDelegate>
{
    @public
    NSWindow* mWindow;
    PDFView* mPdfView;
    NSTextField* mPageLabel;
}
- (void)zoomIn:(id)sender;
- (void)zoomOut:(id)sender;
- (void)goPrev:(id)sender;
- (void)goNext:(id)sender;
- (void)doPrint:(id)sender;
- (void)updatePageLabel;
@end

@implementation WTPDFPreviewController

- (void)zoomIn:(id)sender
{
    (void)sender;
    [mPdfView zoomIn:self];
}

- (void)zoomOut:(id)sender
{
    (void)sender;
    [mPdfView zoomOut:self];
}

- (void)goPrev:(id)sender
{
    (void)sender;
    [mPdfView goToPreviousPage:self];
}

- (void)goNext:(id)sender
{
    (void)sender;
    [mPdfView goToNextPage:self];
}

- (void)doPrint:(id)sender
{
    (void)sender;
    // Prints the exact PDFDocument already shown on screen -- same bytes,
    // no independent re-render.
    [mPdfView printWithInfo:[NSPrintInfo sharedPrintInfo] autoRotate:YES];
}

- (void)updatePageLabel
{
    PDFDocument* doc = mPdfView.document;
    if (!doc)
    {
        mPageLabel.stringValue = @"";
        return;
    }
    NSUInteger total = doc.pageCount;
    PDFPage* currentPage = mPdfView.currentPage;
    NSUInteger current = currentPage ? ([doc indexForPage:currentPage] + 1) : 1;
    mPageLabel.stringValue = [NSString stringWithFormat:@"Page %lu of %lu",
                               (unsigned long)current, (unsigned long)total];
}

- (void)pageChanged:(NSNotification*)note
{
    (void)note;
    [self updatePageLabel];
}

- (void)windowWillClose:(NSNotification*)note
{
    (void)note;
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [NSApp stopModal];
}

@end


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Builds the toolbar strip (zoom/page-nav/print buttons + page label)
/// docked to the top of the content view, above the PDFView.
///
/////////////////////////////////////////////////////////////////////////////
static NSView* BuildToolbar(WTPDFPreviewController* controller, CGFloat width)
{
    const CGFloat toolbarHeight = 36.0;
    NSView* bar = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, width, toolbarHeight)];
    bar.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;

    CGFloat x = 8.0;
    const CGFloat buttonWidth = 80.0;
    const CGFloat buttonHeight = 24.0;
    const CGFloat y = (toolbarHeight - buttonHeight) / 2.0;

    NSButton* zoomOut = [[NSButton alloc] initWithFrame:NSMakeRect(x, y, buttonWidth, buttonHeight)];
    zoomOut.title = @"Zoom Out";
    zoomOut.bezelStyle = NSBezelStyleRounded;
    zoomOut.target = controller;
    zoomOut.action = @selector(zoomOut:);
    [bar addSubview:zoomOut];
    x += buttonWidth + 6.0;

    NSButton* zoomIn = [[NSButton alloc] initWithFrame:NSMakeRect(x, y, buttonWidth, buttonHeight)];
    zoomIn.title = @"Zoom In";
    zoomIn.bezelStyle = NSBezelStyleRounded;
    zoomIn.target = controller;
    zoomIn.action = @selector(zoomIn:);
    [bar addSubview:zoomIn];
    x += buttonWidth + 20.0;

    NSButton* prevPage = [[NSButton alloc] initWithFrame:NSMakeRect(x, y, buttonWidth, buttonHeight)];
    prevPage.title = @"Previous";
    prevPage.bezelStyle = NSBezelStyleRounded;
    prevPage.target = controller;
    prevPage.action = @selector(goPrev:);
    [bar addSubview:prevPage];
    x += buttonWidth + 6.0;

    NSButton* nextPage = [[NSButton alloc] initWithFrame:NSMakeRect(x, y, buttonWidth, buttonHeight)];
    nextPage.title = @"Next";
    nextPage.bezelStyle = NSBezelStyleRounded;
    nextPage.target = controller;
    nextPage.action = @selector(goNext:);
    [bar addSubview:nextPage];
    x += buttonWidth + 20.0;

    const CGFloat labelWidth = 150.0;
    NSTextField* pageLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(x, y + 2.0, labelWidth, 18.0)];
    pageLabel.editable = NO;
    pageLabel.selectable = NO;
    pageLabel.bezeled = NO;
    pageLabel.drawsBackground = NO;
    pageLabel.alignment = NSTextAlignmentLeft;
    [bar addSubview:pageLabel];
    controller->mPageLabel = pageLabel;

    // Print button, right-aligned
    const CGFloat printWidth = 80.0;
    NSButton* printButton = [[NSButton alloc] initWithFrame:
        NSMakeRect(width - printWidth - 8.0, y, printWidth, buttonHeight)];
    printButton.title = @"Print...";
    printButton.bezelStyle = NSBezelStyleRounded;
    printButton.target = controller;
    printButton.action = @selector(doPrint:);
    printButton.autoresizingMask = NSViewMinXMargin;
    [bar addSubview:printButton];

    return bar;
}


/////////////////////////////////////////////////////////////////////////////
void ShowPDFPrintPreview(const std::string& pdfPath, const std::string& windowTitle)
{
    @autoreleasepool
    {
        NSString* nsPath = [NSString stringWithUTF8String:pdfPath.c_str()];
        NSURL* url = [NSURL fileURLWithPath:nsPath];
        PDFDocument* document = [[PDFDocument alloc] initWithURL:url];
        if (!document)
        {
            NSAlert* alert = [[NSAlert alloc] init];
            alert.messageText = @"Print Preview";
            alert.informativeText = @"Failed to load the generated PDF file.";
            [alert runModal];
            return;
        }

        const CGFloat windowWidth = 760.0;
        const CGFloat windowHeight = 900.0;
        const CGFloat toolbarHeight = 36.0;

        NSRect frame = NSMakeRect(0, 0, windowWidth, windowHeight);
        NSWindow* window = [[NSWindow alloc]
            initWithContentRect:frame
                      styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                 NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
                        backing:NSBackingStoreBuffered
                          defer:NO];
        window.title = [NSString stringWithUTF8String:windowTitle.c_str()];
        [window center];

        WTPDFPreviewController* controller = [[WTPDFPreviewController alloc] init];
        controller->mWindow = window;
        window.delegate = controller;

        NSView* contentView = window.contentView;
        contentView.autoresizesSubviews = YES;

        PDFView* pdfView = [[PDFView alloc]
            initWithFrame:NSMakeRect(0, 0, windowWidth, windowHeight - toolbarHeight)];
        pdfView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        pdfView.document = document;
        pdfView.autoScales = YES;
        pdfView.displayMode = kPDFDisplaySinglePageContinuous;
        [contentView addSubview:pdfView];
        controller->mPdfView = pdfView;

        NSView* toolbar = BuildToolbar(controller, windowWidth);
        toolbar.frame = NSMakeRect(0, windowHeight - toolbarHeight, windowWidth, toolbarHeight);
        [contentView addSubview:toolbar];

        [controller updatePageLabel];
        [[NSNotificationCenter defaultCenter] addObserver:controller
                                                   selector:@selector(pageChanged:)
                                                       name:PDFViewPageChangedNotification
                                                     object:pdfView];

        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        // Modal: blocks until windowWillClose: calls [NSApp stopModal],
        // matching the QPrintPreviewDialog::exec() behaviour this replaces.
        [NSApp runModalForWindow:window];
    }
}
