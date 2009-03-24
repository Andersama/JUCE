/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-7 by Raw Material Software ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the
   GNU General Public License, as published by the Free Software Foundation;
   either version 2 of the License, or (at your option) any later version.

   JUCE is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with JUCE; if not, visit www.gnu.org/licenses or write to the
   Free Software Foundation, Inc., 59 Temple Place, Suite 330,
   Boston, MA 02111-1307 USA

  ------------------------------------------------------------------------------

   If you'd like to release a closed-source product which uses JUCE, commercial
   licenses are also available: visit www.rawmaterialsoftware.com/juce for
   more information.

  ==============================================================================
*/

// (This file gets included by juce_mac_NativeCode.mm, rather than being
// compiled on its own).
#ifdef JUCE_INCLUDED_FILE

struct CallbackMessagePayload
{
    MessageCallbackFunction* function;
    void* parameter;
    void* volatile result;
    bool volatile hasBeenExecuted;
};

/* When you use multiple DLLs which share similarly-named obj-c classes - like
   for example having more than one juce plugin loaded into a host, then when a
   method is called, the actual code that runs might actually be in a different module
   than the one you expect... So any calls to library functions or statics that are
   made inside obj-c methods will probably end up getting executed in a different DLL's
   memory space. Not a great thing to happen - this obviously leads to bizarre crashes.

   To work around this insanity, I'm only allowing obj-c methods to make calls to
   virtual methods of an object that's known to live inside the right module's space.
*/
class AppDelegateRedirector
{
public:
    AppDelegateRedirector() {}
    virtual ~AppDelegateRedirector() {}

    virtual NSApplicationTerminateReply shouldTerminate()
    {
        if (JUCEApplication::getInstance() != 0)
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
            return NSTerminateCancel;
        }

        return NSTerminateNow;
    }

    virtual BOOL openFile (const NSString* filename)
    {
        if (JUCEApplication::getInstance() != 0)
        {
            JUCEApplication::getInstance()->anotherInstanceStarted (nsStringToJuce (filename));
            return YES;
        }

        return NO;
    }

    virtual void openFiles (NSArray* filenames)
    {
        StringArray files;
        for (unsigned int i = 0; i < [filenames count]; ++i)
            files.add (nsStringToJuce ((NSString*) [filenames objectAtIndex: i]));

        if (files.size() > 0 && JUCEApplication::getInstance() != 0)
        {
            JUCEApplication::getInstance()->anotherInstanceStarted (files.joinIntoString (T(" ")));
        }
    }

    virtual void focusChanged()
    {
        juce_HandleProcessFocusChange();
    }

    virtual void deliverMessage (void* message)
    {
        // no need for an mm lock here - deliverMessage locks it
        MessageManager::getInstance()->deliverMessage (message);
    }

    virtual void performCallback (CallbackMessagePayload* pl)
    {
        pl->result = (*pl->function) (pl->parameter);
        pl->hasBeenExecuted = true;
    }

    virtual void deleteSelf()
    {
        delete this;
    }
};

END_JUCE_NAMESPACE
using namespace JUCE_NAMESPACE;

#define JuceAppDelegate MakeObjCClassName(JuceAppDelegate)

static int numPendingMessages = 0;
static bool flushingMessages = false;

@interface JuceAppDelegate   : NSObject
{
@private
    id oldDelegate;
    AppDelegateRedirector* redirector;
}

- (JuceAppDelegate*) init;
- (void) dealloc;
- (BOOL) application: (NSApplication*) theApplication openFile: (NSString*) filename;
- (void) application: (NSApplication*) sender openFiles: (NSArray*) filenames;
- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication*) app;
- (void) applicationDidBecomeActive: (NSNotification*) aNotification;
- (void) applicationDidResignActive: (NSNotification*) aNotification;
- (void) applicationWillUnhide: (NSNotification*) aNotification;
- (void) customEvent: (id) data;
- (void) performCallback: (id) info;
- (void) dummyMethod;
@end

@implementation JuceAppDelegate

- (JuceAppDelegate*) init
{
    [super init];

    redirector = new AppDelegateRedirector();
    numPendingMessages = 0;
    flushingMessages = false;

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];

    if (JUCEApplication::getInstance() != 0)
    {
        oldDelegate = [NSApp delegate];
        [NSApp setDelegate: self];
    }
    else
    {
        oldDelegate = 0;
        [center addObserver: self selector: @selector (applicationDidResignActive:)
                       name: NSApplicationDidResignActiveNotification object: NSApp];

        [center addObserver: self selector: @selector (applicationDidBecomeActive:)
                       name: NSApplicationDidBecomeActiveNotification object: NSApp];

        [center addObserver: self selector: @selector (applicationWillUnhide:)
                       name: NSApplicationWillUnhideNotification object: NSApp];

    }

    return self;
}

- (void) dealloc
{
    if (oldDelegate != 0)
        [NSApp setDelegate: oldDelegate];

    redirector->deleteSelf();
    [super dealloc];
}

- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication*) app
{
    return redirector->shouldTerminate();
}

- (BOOL) application: (NSApplication*) app openFile: (NSString*) filename
{
    return redirector->openFile (filename);
}

- (void) application: (NSApplication*) sender openFiles: (NSArray*) filenames
{
    return redirector->openFiles (filenames);
}

- (void) applicationDidBecomeActive: (NSNotification*) aNotification
{
    redirector->focusChanged();
}

- (void) applicationDidResignActive: (NSNotification*) aNotification
{
    redirector->focusChanged();
}

- (void) applicationWillUnhide: (NSNotification*) aNotification
{
    redirector->focusChanged();
}

- (void) customEvent: (id) n
{
    atomicDecrement (numPendingMessages);

    NSData* data = (NSData*) n;
    void* message = 0;
    [data getBytes: &message length: sizeof (message)];

    if (message != 0 && ! flushingMessages)
        redirector->deliverMessage (message);

    [data release];
}

- (void) performCallback: (id) info
{
    if ([info isKindOfClass: [NSData class]])
    {
        CallbackMessagePayload* pl = (CallbackMessagePayload*) [((NSData*) info) bytes];

        if (pl != 0)
            redirector->performCallback (pl);
    }
    else
    {
        jassertfalse // should never get here!
    }
}

- (void) dummyMethod  {}   // (used as a way of running a dummy thread)

@end

BEGIN_JUCE_NAMESPACE

static JuceAppDelegate* juceAppDelegate = 0;

void MessageManager::runDispatchLoop()
{
    if (! quitMessagePosted) // check that the quit message wasn't already posted..
    {
        const ScopedAutoReleasePool pool;

        // must only be called by the message thread!
        jassert (isThisTheMessageThread());

        [NSApp run];
    }
}

void MessageManager::stopDispatchLoop()
{
    quitMessagePosted = true;
    [NSApp stop: nil];
}

bool MessageManager::runDispatchLoopUntil (int millisecondsToRunFor)
{
    const ScopedAutoReleasePool pool;
    jassert (isThisTheMessageThread()); // must only be called by the message thread

    uint32 endTime = Time::getMillisecondCounter() + millisecondsToRunFor;
    NSDate* endDate = [NSDate dateWithTimeIntervalSinceNow: millisecondsToRunFor * 0.001];

    while (Time::getMillisecondCounter() < endTime && ! quitMessagePosted)
    {
        const ScopedAutoReleasePool pool;

        [[NSRunLoop currentRunLoop] runMode: NSDefaultRunLoopMode
                                 beforeDate: endDate];

        NSEvent* e = [NSApp nextEventMatchingMask: NSAnyEventMask
                                        untilDate: endDate
                                           inMode: NSDefaultRunLoopMode
                                          dequeue: YES];
        [NSApp sendEvent: e];
    }

    return ! quitMessagePosted;
}

//==============================================================================
void MessageManager::doPlatformSpecificInitialisation()
{
    if (juceAppDelegate == 0)
        juceAppDelegate = [[JuceAppDelegate alloc] init];

    // This launches a dummy thread, which forces Cocoa to initialise NSThreads
    // correctly (needed prior to 10.5)
    if (! [NSThread isMultiThreaded])
        [NSThread detachNewThreadSelector: @selector (dummyMethod)
                                 toTarget: juceAppDelegate
                               withObject: nil];

    initialiseMainMenu();
}

void MessageManager::doPlatformSpecificShutdown()
{
    [[NSRunLoop currentRunLoop] cancelPerformSelectorsWithTarget: juceAppDelegate];
    [[NSNotificationCenter defaultCenter] removeObserver: juceAppDelegate];

    // Annoyingly, cancelPerformSelectorsWithTarget can't actually cancel the messages
    // sent by performSelectorOnMainThread, so need to manually flush these before quitting..
    for (int i = 100; --i >= 0 && numPendingMessages > 0;)
    {
        flushingMessages = true;
        getInstance()->runDispatchLoopUntil (10);
    }

    jassert (numPendingMessages == 0); // failed to get all the pending messages cleared before quitting..

    [juceAppDelegate release];
    juceAppDelegate = 0;
}

bool juce_postMessageToSystemQueue (void* message)
{
    atomicIncrement (numPendingMessages);

    [juceAppDelegate performSelectorOnMainThread: @selector (customEvent:)
                     withObject: (id) [[NSData alloc] initWithBytes: &message
                                                             length: (int) sizeof (message)]
                     waitUntilDone: NO];
    return true;
}

void MessageManager::broadcastMessage (const String& value) throw()
{
}

void* MessageManager::callFunctionOnMessageThread (MessageCallbackFunction* callback,
                                                   void* data)
{
    if (isThisTheMessageThread())
    {
        return (*callback) (data);
    }
    else
    {
        // If a thread has a MessageManagerLock and then tries to call this method, it'll
        // deadlock because the message manager is blocked from running, so can never
        // call your function..
        jassert (! MessageManager::getInstance()->currentThreadHasLockedMessageManager());

        const ScopedAutoReleasePool pool;

        CallbackMessagePayload cmp;
        cmp.function = callback;
        cmp.parameter = data;
        cmp.result = 0;
        cmp.hasBeenExecuted = false;

        [juceAppDelegate performSelectorOnMainThread: @selector (performCallback:)
                                          withObject: [NSData dataWithBytesNoCopy: &cmp
                                                                           length: sizeof (cmp)
                                                                     freeWhenDone: NO]
                                       waitUntilDone: YES];

        return cmp.result;
    }
}

#endif
