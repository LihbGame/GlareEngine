////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#import "ViewController.h"
#import "DisplayView.h"
#import "UIKitDisplay.h"


using namespace Noesis;
using namespace NoesisApp;


@interface ViewController()
{
UIKitDisplay* display;
}
@end


@implementation ViewController

- (id)initWithDisplay:(UIKitDisplay*)display_
{
	self = [super init];
    display = display_;
    return self;
}

- (void)loadView
{
    CGRect bounds = [[UIScreen mainScreen] bounds];
    DisplayView* view = [[[DisplayView alloc] initWithFrame:bounds] autorelease];
    view->display = display;
    view.multipleTouchEnabled = YES;
    self.view = view;
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
}

- (NSUInteger)supportedInterfaceOrientations
{
    return UIInterfaceOrientationMaskAll;
}

- (BOOL)shouldAutorotate
{
    return YES;
}

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext> context)
    {
    } completion:^(id<UIViewControllerTransitionCoordinatorContext> context)
    {
        display->OnSizeChanged(display->GetClientWidth(), display->GetClientHeight());
    }];
}

@end
