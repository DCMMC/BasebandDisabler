#import "BDBController.h"
#import "BDBBridge.h"
static NSString *L(NSString *key){return [[NSBundle bundleForClass:BDBController.class] localizedStringForKey:key value:key table:nil];}
@interface BDBController ()
@property NSDictionary *state;
@property BOOL busy;
@property UILabel *headline;
@property UILabel *summary;
@property UIImageView *symbol;
@end
@implementation BDBController
- (instancetype)init {return [super initWithStyle:UITableViewStyleInsetGrouped];}
- (void)viewDidLoad {
    [super viewDidLoad];self.title=@"Baseband Disabler";
    self.navigationItem.rightBarButtonItem=[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemRefresh target:self action:@selector(refresh)];
    self.tableView.rowHeight=64;self.tableView.backgroundColor=UIColor.systemGroupedBackgroundColor;
    UIView *header=[[UIView alloc] initWithFrame:CGRectMake(0,0,320,230)];
    self.symbol=[[UIImageView alloc] initWithImage:[UIImage systemImageNamed:@"antenna.radiowaves.left.and.right.slash"]];
    self.symbol.contentMode=UIViewContentModeScaleAspectFit;
    self.headline=[UILabel new];self.headline.font=[UIFont preferredFontForTextStyle:UIFontTextStyleTitle1];self.headline.textAlignment=NSTextAlignmentCenter;self.headline.numberOfLines=0;
    self.summary=[UILabel new];self.summary.font=[UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];self.summary.textColor=UIColor.secondaryLabelColor;self.summary.textAlignment=NSTextAlignmentCenter;self.summary.numberOfLines=0;
    UIStackView *stack=[[UIStackView alloc] initWithArrangedSubviews:@[self.symbol,self.headline,self.summary]];stack.axis=UILayoutConstraintAxisVertical;stack.spacing=14;stack.translatesAutoresizingMaskIntoConstraints=NO;[header addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[[stack.leadingAnchor constraintEqualToAnchor:header.leadingAnchor constant:28],[stack.trailingAnchor constraintEqualToAnchor:header.trailingAnchor constant:-28],[stack.topAnchor constraintEqualToAnchor:header.topAnchor constant:26],[stack.bottomAnchor constraintLessThanOrEqualToAnchor:header.bottomAnchor constant:-20],[self.symbol.heightAnchor constraintEqualToConstant:54]]];
    self.tableView.tableHeaderView=header;[self render];
}
- (void)viewWillAppear:(BOOL)animated {[super viewWillAppear:animated];[self refresh];}
- (void)render {
    BOOL ready=[self.state[@"supported"] boolValue],off=[self.state[@"hardware_off"] boolValue],missing=[self.state[@"failure_detected"] boolValue];
    self.headline.text=L(self.busy?@"checking":(!self.state?@"unavailable":(!ready?@"unsupported":(off?@"off":(missing?@"on":@"healthy")))));
    self.summary.text=L(!ready?@"compatibility":(off?@"off_detail":(missing?@"on_detail":@"healthy_detail")));
    self.symbol.tintColor=ready && off?UIColor.systemGreenColor:UIColor.systemBlueColor;
    self.navigationItem.rightBarButtonItem.enabled=!self.busy;
    [self.tableView reloadData];
}
- (void)refresh {if(!self.busy)[self request:@[@"status"] notify:NO];}
- (void)request:(NSArray *)arguments notify:(BOOL)notify {
    if(self.busy)return;self.busy=YES;[self render];
    __weak typeof(self) weakSelf=self;
    BDBRequest(arguments,^(NSDictionary *response){
        typeof(self) self=weakSelf;if(!self)return;self.busy=NO;
        if([response[@"status"] isKindOfClass:NSDictionary.class] && [response[@"status"] count])self.state=response[@"status"];
        [self render];
        if(![response[@"ok"] boolValue]){
            NSString *key=[@"error." stringByAppendingString:response[@"error"]?:@"operation_failed"];
            NSString *message=L(key);if([message isEqual:key])message=L(@"error.operation_failed");
            UIAlertController *alert=[UIAlertController alertControllerWithTitle:L(@"failed") message:message preferredStyle:UIAlertControllerStyleAlert];
            [alert addAction:[UIAlertAction actionWithTitle:L(@"ok") style:UIAlertActionStyleDefault handler:nil]];[self presentViewController:alert animated:YES completion:nil];
        } else if(notify){UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,L(@"completed"));}
    });
}
- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {return 3;}
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {return section==2?2:1;}
- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {return L(@[@"control",@"automatic",@"about"][section]);}
- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section {
    return L(@[@"control_footer",@"auto_footer",@"about_footer"][section]);
}
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    UITableViewCell *cell=[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:nil];
    cell.textLabel.font=[UIFont preferredFontForTextStyle:UIFontTextStyleBody];cell.textLabel.adjustsFontForContentSizeCategory=YES;cell.textLabel.numberOfLines=0;
    BOOL ready=[self.state[@"supported"] boolValue],off=[self.state[@"hardware_off"] boolValue];
    if(indexPath.section==0){
        cell.textLabel.text=L(off?@"restore":@"disable");cell.imageView.image=[UIImage systemImageNamed:off?@"arrow.uturn.backward.circle":@"power.circle.fill"];
        BOOL enabled=!self.busy && ready && (off || [self.state[@"failure_detected"] boolValue]);cell.userInteractionEnabled=enabled;cell.textLabel.textColor=enabled?UIColor.systemBlueColor:UIColor.tertiaryLabelColor;cell.imageView.tintColor=cell.textLabel.textColor;
        if(self.busy){UIActivityIndicatorView *spinner=[[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];[spinner startAnimating];cell.accessoryView=spinner;}
    } else if(indexPath.section==1){
        cell.textLabel.text=L(@"auto");UISwitch *toggle=[UISwitch new];toggle.on=[self.state[@"auto_enabled"] boolValue];toggle.enabled=!self.busy && self.state && (toggle.on || (ready && [self.state[@"failure_detected"] boolValue]));[toggle addTarget:self action:@selector(autoChanged:) forControlEvents:UIControlEventValueChanged];cell.accessoryView=toggle;cell.selectionStyle=UITableViewCellSelectionStyleNone;
    } else if(indexPath.row==0){
        cell.textLabel.text=L(@"device");cell.detailTextLabel.text=[NSString stringWithFormat:@"%@ · %@",self.state[@"model"]?:@"—",self.state[@"build"]?:@"—"];cell.selectionStyle=UITableViewCellSelectionStyleNone;
    } else {cell.textLabel.text=L(@"copy");cell.imageView.image=[UIImage systemImageNamed:@"doc.on.doc"];cell.textLabel.textColor=UIColor.systemBlueColor;}
    return cell;
}
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)path {
    [tableView deselectRowAtIndexPath:path animated:YES];
    if(path.section==0)[self request:@[[self.state[@"hardware_off"] boolValue]?@"restore":@"disable"] notify:YES];
    if(path.section==2 && path.row==1 && self.state){NSData *data=[NSJSONSerialization dataWithJSONObject:self.state options:NSJSONWritingPrettyPrinted error:nil];UIPasteboard.generalPasteboard.string=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];UIAlertController *alert=[UIAlertController alertControllerWithTitle:L(@"copied") message:nil preferredStyle:UIAlertControllerStyleAlert];[alert addAction:[UIAlertAction actionWithTitle:L(@"ok") style:UIAlertActionStyleDefault handler:nil]];[self presentViewController:alert animated:YES completion:nil];}
}
- (void)autoChanged:(UISwitch *)sender {[self request:@[@"auto",sender.on?@"on":@"off"] notify:YES];}
@end
