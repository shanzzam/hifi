#import "Launcher.h"
#import "Window.h"
#import "SplashScreen.h"
#import "LoginScreen.h"
#import "DisplayNameScreen.h"
#import "ProcessScreen.h"
#import "ErrorViewController.h"
#import "Settings.h"

@interface Launcher ()

@property (strong) IBOutlet Window* window;
@property (nonatomic, assign) NSString* finalFilePath;
@property (nonatomic, assign) NSButton* button;
@end


static BOOL const DELETE_ZIP_FILES = TRUE;
@implementation Launcher
+ (id) sharedLauncher {
    static Launcher* sharedLauncher = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedLauncher = [[self alloc]init];
    });
    return sharedLauncher;
}

-(id)init {
    if (self = [super init]) {
        self.username = [[NSString alloc] initWithString:@"Default Property Value"];
        self.downloadInterface = [DownloadInterface alloc];
        self.downloadDomainContent = [DownloadDomainContent alloc];
        self.credentialsRequest = [CredentialsRequest alloc];
        self.latestBuildRequest = [LatestBuildRequest alloc];
        self.organizationRequest = [OrganizationRequest alloc];
        self.downloadScripts = [DownloadScripts alloc];
        self.credentialsAccepted = TRUE;
        self.gotCredentialResponse = FALSE;
        self.waitingForCredentialReponse = FALSE;
        self.waitingForInterfaceToTerminate = FALSE;
        self.userToken = nil;
        self.processState = DOWNLOADING_INTERFACE;
    }
    return self;
}

-(void)awakeFromNib {
    [[NSApplication sharedApplication] activateIgnoringOtherApps:TRUE];
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                                                           selector:@selector(didTerminateApp:)
                                                               name:NSWorkspaceDidTerminateApplicationNotification
                                                             object:nil];
    
    SplashScreen* splashScreen = [[SplashScreen alloc] initWithNibName:@"SplashScreen" bundle:nil];
    [self.window setContentViewController: splashScreen];
    [self closeInterfaceIfRunning];
    
    if (!self.waitingForInterfaceToTerminate) {
        [self checkLoginStatus];
    }
}

- (NSString*) getDownloadPathForContentAndScripts
{
    NSString* filePath = [[NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES) objectAtIndex:0]
                          stringByAppendingString:@"/Launcher/"];
    
    if (![[NSFileManager defaultManager] fileExistsAtPath:filePath]) {
        NSError * error = nil;
        [[NSFileManager defaultManager] createDirectoryAtPath:filePath withIntermediateDirectories:TRUE attributes:nil error:&error];
    }
    
    return filePath;
}

- (NSString*) getLauncherPath
{
    return [[[NSBundle mainBundle] bundlePath] stringByAppendingString:@"/Contents/MacOS/"];
}

- (void) extractZipFileAtDestination:(NSString *)destination :(NSString*)file
{
    NSTask* task = [[NSTask alloc] init];
    task.launchPath = @"/usr/bin/unzip";
    task.arguments = @[@"-o", @"-d", destination, file];
    
    [task launch];
    [task waitUntilExit];
    
    if (DELETE_ZIP_FILES) {
        NSFileManager* fileManager = [NSFileManager defaultManager];
        [fileManager removeItemAtPath:file error:NULL];
    }
}

- (void) displayErrorPage
{
    ErrorViewController* errorPage = [[ErrorViewController alloc] initWithNibName:@"ErrorScreen" bundle:nil];
    [[[[NSApplication sharedApplication] windows] objectAtIndex:0] setContentViewController: errorPage];
}

- (void) checkLoginStatus
{
    if ([self isLoadedIn]) {
        Launcher* sharedLauncher = [Launcher sharedLauncher];
        [sharedLauncher setCurrentProcessState:CHECKING_UPDATE];
        ProcessScreen* processScreen = [[ProcessScreen alloc] initWithNibName:@"ProcessScreen" bundle:nil];
        [[[[NSApplication sharedApplication] windows] objectAtIndex:0] setContentViewController: processScreen];
        [self.latestBuildRequest requestLatestBuildInfo];
    } else {
        [NSTimer scheduledTimerWithTimeInterval:2.0
                                         target:self
                                       selector:@selector(onSplashScreenTimerFinished:)
                                       userInfo:nil
                                        repeats:NO];
    }
    [[NSApplication sharedApplication] activateIgnoringOtherApps:TRUE];
}

- (void) setDownloadContextFilename:(NSString *)aFilename
{
    self.contentFilename = aFilename;
}

- (void) setDownloadScriptsFilename:(NSString*)aFilename
{
    self.scriptsFilename = aFilename;
}

- (NSString*) getDownloadContentFilename
{
    return self.contentFilename;
}

- (NSString*) getDownloadScriptsFilename
{
    return self.scriptsFilename;
}

- (void)didTerminateApp:(NSNotification *)notification {
    if (self.waitingForInterfaceToTerminate) {
        NSString* appName = [notification.userInfo valueForKey:@"NSApplicationName"];
        if ([appName isEqualToString:@"interface"]) {
            self.waitingForInterfaceToTerminate = FALSE;
            [self checkLoginStatus];
        }
    }
}

- (void) closeInterfaceIfRunning
{
    NSWorkspace* workspace = [NSWorkspace sharedWorkspace];
    NSArray* apps = [workspace runningApplications];
    for (NSRunningApplication* app in apps) {
        if ([[app bundleIdentifier] isEqualToString:@"com.highfidelity.interface"] ||
            [[app bundleIdentifier] isEqualToString:@"com.highfidelity.interface-pr"]) {
            [app terminate];
            self.waitingForInterfaceToTerminate = true;
        }
    }
}

- (BOOL) isWaitingForInterfaceToTerminate {
    return self.waitingForInterfaceToTerminate;
}

- (BOOL) isLoadedIn
{
    return [[Settings sharedSettings] isLoggedIn];
}

- (void) setDomainURLInfo:(NSString *)aDomainURL :(NSString *)aDomainContentUrl :(NSString *)aDomainScriptsUrl
{
    self.domainURL = aDomainURL;
    self.domainContentUrl = aDomainContentUrl;
    self.domainScriptsUrl = aDomainScriptsUrl;
    
    [[Settings sharedSettings] setDomainUrl:aDomainURL];
}

- (NSString*) getAppPath
{
    return [self getDownloadPathForContentAndScripts];
}

- (BOOL) loginShouldSetErrorState
{
    return !self.credentialsAccepted;
}

- (void) displayNameEntered:(NSString*)aDiplayName
{
    self.processState = DOWNLOADING_INTERFACE;
    ProcessScreen* processScreen = [[ProcessScreen alloc] initWithNibName:@"ProcessScreen" bundle:nil];
    [[[[NSApplication sharedApplication] windows] objectAtIndex:0] setContentViewController: processScreen];
    [self.downloadDomainContent downloadDomainContent:self.domainContentUrl];
    self.displayName = aDiplayName;
}

- (void) domainContentDownloadFinished
{
    //.[self.downloadScripts downloadScripts:self.domainScriptsUrl];
    [self.latestBuildRequest requestLatestBuildInfo];
}

- (void) domainScriptsDownloadFinished
{
    [self.latestBuildRequest requestLatestBuildInfo];
}

- (void) saveCredentialsAccepted:(BOOL)aCredentialsAccepted
{
    self.credentialsAccepted = aCredentialsAccepted;
}

- (void) credentialsAccepted:(BOOL)aCredentialsAccepted
{
    self.credentialsAccepted = aCredentialsAccepted;
    if (aCredentialsAccepted) {
        DisplayNameScreen* displayNameScreen = [[DisplayNameScreen alloc] initWithNibName:@"DisplayNameScreen" bundle:nil];
        [[[[NSApplication sharedApplication] windows] objectAtIndex:0] setContentViewController: displayNameScreen];
    } else {
        LoginScreen* loginScreen = [[LoginScreen alloc] initWithNibName:@"LoginScreen" bundle:nil];
        [[[[NSApplication sharedApplication] windows] objectAtIndex:0] setContentViewController: loginScreen];
    }
}

- (void) interfaceFinishedDownloading
{
    if (self.processState == DOWNLOADING_INTERFACE) {
        self.processState = RUNNING_INTERFACE_AFTER_DOWNLOAD;
    } else {
        self.processState = RUNNING_INTERFACE_AFTER_UPDATE;
    }
    ProcessScreen* processScreen = [[ProcessScreen alloc] initWithNibName:@"ProcessScreen" bundle:nil];
    [[[[NSApplication sharedApplication] windows] objectAtIndex:0] setContentViewController: processScreen];
    [self launchInterface];
}

- (void) credentialsEntered:(NSString*)aOrginization :(NSString*)aUsername :(NSString*)aPassword
{
    self.organization = aOrginization;
    self.username = aUsername;
    self.password = aPassword;
    [self.organizationRequest confirmOrganization:aOrginization :aUsername];
}

- (void) organizationRequestFinished:(BOOL)aOriginzationAccepted
{
    self.credentialsAccepted = aOriginzationAccepted;
    if (aOriginzationAccepted) {
        [self.credentialsRequest confirmCredentials:self.username : self.password];
    } else {
        LoginScreen* loginScreen = [[LoginScreen alloc] initWithNibName:@"LoginScreen" bundle:nil];
        [[[[NSApplication sharedApplication] windows] objectAtIndex:0] setContentViewController: loginScreen];
    }
}

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

-(void) showLoginScreen
{
    LoginScreen* loginScreen = [[LoginScreen alloc] initWithNibName:@"LoginScreen" bundle:nil];
    [[[[NSApplication sharedApplication] windows] objectAtIndex:0] setContentViewController: loginScreen];
}

- (void) shouldDownloadLatestBuild:(BOOL) shouldDownload :(NSString*) downloadUrl
{
    if (shouldDownload) {
        [self.downloadInterface downloadInterface: downloadUrl];
        return;
    }
    [self launchInterface];
}

-(void)onSplashScreenTimerFinished:(NSTimer *)timer
{
    [[NSApplication sharedApplication] activateIgnoringOtherApps:TRUE];
    [self showLoginScreen];
}

-(void)setCurrentProcessState:(ProcessState)aProcessState
{
    self.processState = aProcessState;
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification
{
    [self.window makeKeyAndOrderFront:self];
}

- (void) setDownloadFilename:(NSString *)aFilename
{
    self.filename = aFilename;
}

- (NSString*) getDownloadFilename
{
    return self.filename;
}

- (void) setTokenString:(NSString *)aToken
{
    self.userToken = aToken;
}

- (NSString*) getTokenString
{
    return self.userToken;
}

- (void) setLoginErrorState:(LoginError)aLoginError
{
    self.loginError = aLoginError;
}

- (LoginError) getLoginErrorState
{
    return self.loginError;
}

- (void) launchInterface
{
    NSString* launcherPath = [[self getLauncherPath] stringByAppendingString:@"HQ Launcher"];
    
    [[Settings sharedSettings] setLauncherPath:launcherPath];
    [[Settings sharedSettings] save];
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSURL *url = [NSURL fileURLWithPath:[workspace fullPathForApplication:[[self getAppPath] stringByAppendingString:@"interface.app/Contents/MacOS/interface"]]];

    NSError *error = nil;
    
    NSString* contentPath = [[self getDownloadPathForContentAndScripts] stringByAppendingString:@"content"];
    NSString* displayName = [ self displayName];
    NSString* scriptsPath = [[self getAppPath] stringByAppendingString:@"interface.app/Contents/Resources/scripts/simplifiedUI/"];
    NSString* domainUrl = [[Settings sharedSettings] getDomainUrl];
    NSString* userToken = [[Launcher sharedLauncher] getTokenString];
    NSString* homeBookmark = [[NSString stringWithFormat:@"hqhome="] stringByAppendingString:domainUrl];
    NSArray* arguments;
    if (userToken != nil) {
        arguments = [NSArray arrayWithObjects:
                        @"--url" , domainUrl ,
                        @"--tokens", userToken,
                        @"--cache", contentPath,
                        @"--displayName", displayName,
                        @"--scripts", scriptsPath,
                        @"--setBookmark", homeBookmark,
                        @"--no-updater",
                        @"--no-launcher", nil];
    } else {
        arguments = [NSArray arrayWithObjects:
                            @"--url" , domainUrl,
                            @"--cache", contentPath,
                            @"--scripts", scriptsPath,
                            @"--setBookmark", homeBookmark,
                            @"--no-updater",
                            @"--no-launcher", nil];
    }
    [workspace launchApplicationAtURL:url options:NSWorkspaceLaunchNewInstance configuration:[NSDictionary dictionaryWithObject:arguments forKey:NSWorkspaceLaunchConfigurationArguments] error:&error];
    
    [NSApp terminate:self];
}

- (ProcessState) currentProccessState
{
    return self.processState;
}

@end
