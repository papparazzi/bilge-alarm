# Local-only helper: polls <this folder>/inbox for *.py, runs each one inside Fusion on the main
# thread, writes stdout + any traceback to <this folder>/outbox/<name>.log, then moves the file to done/.
#
# v2: a script is claimed (moved to running/) BEFORE it executes and a busy flag blocks re-entry.
# v1 left the file in inbox/ while it ran; Fusion services events during slow calls (STEP import), so the
# same script was started again inside itself, over and over.
import adsk.core, traceback, threading, os, io, time, shutil, contextlib

HERE = os.path.dirname(os.path.abspath(__file__))
INBOX, OUTBOX, DONE, RUNNING = (os.path.join(HERE, d) for d in ('inbox', 'outbox', 'done', 'running'))
EVENT_ID = 'ClaudeBridgePoll'
_app = None
_handlers = []
_stop = threading.Event()
_event = None
_busy = False


class PollHandler(adsk.core.CustomEventHandler):
    def notify(self, args):
        global _busy
        if _busy:
            return
        _busy = True
        try:
            for d in (INBOX, OUTBOX, DONE, RUNNING):
                os.makedirs(d, exist_ok=True)
            for fn in sorted(os.listdir(INBOX)):
                if not fn.endswith('.py'):
                    continue
                claimed = os.path.join(RUNNING, fn)
                try:
                    shutil.move(os.path.join(INBOX, fn), claimed)      # claim first: nothing can run it twice
                except Exception:
                    continue
                buf = io.StringIO()
                with contextlib.redirect_stdout(buf):
                    try:
                        with open(claimed, 'r', encoding='utf-8') as f:
                            code = compile(f.read(), claimed, 'exec')
                        exec(code, {'__name__': '__claude__', '__file__': claimed})
                    except Exception:
                        print('EXCEPTION\n' + traceback.format_exc())
                with open(os.path.join(OUTBOX, fn + '.log'), 'w', encoding='utf-8') as f:
                    f.write(buf.getvalue() or '(no output)')
                shutil.move(claimed, os.path.join(DONE, '%d_%s' % (time.time(), fn)))
        finally:
            _busy = False


def _poll_loop():
    while not _stop.wait(1.0):
        try:
            if not _busy and os.path.isdir(INBOX) and any(f.endswith('.py') for f in os.listdir(INBOX)):
                _app.fireCustomEvent(EVENT_ID, '')
        except Exception:
            pass


def run(context):
    global _app, _event
    _app = adsk.core.Application.get()
    try:
        _app.unregisterCustomEvent(EVENT_ID)
    except Exception:
        pass
    _event = _app.registerCustomEvent(EVENT_ID)
    h = PollHandler()
    _event.add(h)
    _handlers.append(h)
    threading.Thread(target=_poll_loop, daemon=True).start()
    _app.userInterface.messageBox('ClaudeBridge v2 running.\nWatching: ' + INBOX)


def stop(context):
    _stop.set()
    try:
        _app.unregisterCustomEvent(EVENT_ID)
    except Exception:
        pass
