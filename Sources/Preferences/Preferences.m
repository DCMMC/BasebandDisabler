#import <UIKit/UIKit.h>
#import "../App/BDBController.h"
// Preferences supplies this private base class at runtime; no private headers are bundled.
@interface PSViewController : UIViewController
@end
@interface BDBPreferencesController : PSViewController
@property BDBController *content;
@end
@implementation BDBPreferencesController
- (void)loadView {self.view=[[UIView alloc] initWithFrame:UIScreen.mainScreen.bounds];}
- (void)viewDidLoad {
    [super viewDidLoad];self.title=@"Baseband Disabler";self.content=[BDBController new];
    [self addChildViewController:self.content];self.content.view.frame=self.view.bounds;self.content.view.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;[self.view addSubview:self.content.view];[self.content didMoveToParentViewController:self];
    self.navigationItem.rightBarButtonItem=[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemRefresh target:self.content action:@selector(refresh)];
    [self.content refresh];
}
@end
