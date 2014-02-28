status_t SurfaceFlinger::captureScreenImplLocked(DisplayID dpy,
        sp<IMemoryHeap>* heap,
        uint32_t* w, uint32_t* h, PixelFormat* f,
        uint32_t sw, uint32_t sh)
{
   LOGI("captureScreenImplLocked");
    status_t result = PERMISSION_DENIED;

    // only one display supported for now
    if (UNLIKELY(uint32_t(dpy) >= DISPLAY_COUNT))
        return BAD_VALUE;

    if (!GLExtensions::getInstance().haveFramebufferObject())
        return INVALID_OPERATION;

    // get screen geometry
    const DisplayHardware& hw(graphicPlane(dpy).displayHardware());
    const uint32_t hw_w = hw.getWidth();
    const uint32_t hw_h = hw.getHeight();

    if ((sw > hw_w) || (sh > hw_h))
        return BAD_VALUE;

    sw = (!sw) ? hw_w : sw;
    sh = (!sh) ? hw_h : sh;
    const size_t size = sw * sh * 4;

    // make sure to clear all GL error flags
    while ( glGetError() != GL_NO_ERROR ) ;

    // create a FBO
    GLuint name, tname;
    glGenRenderbuffersOES(1, &tname);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, tname);
    glRenderbufferStorageOES(GL_RENDERBUFFER_OES, GL_RGBA8_OES, sw, sh);
    glGenFramebuffersOES(1, &name);
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, name);
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES,
            GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, tname);

    GLenum status = glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES);
    if (status == GL_FRAMEBUFFER_COMPLETE_OES) {

        // invert everything, b/c glReadPixel() below will invert the FB
        glViewport(0, 0, sw, sh);
        glScissor(0, 0, sw, sh);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrthof(0, hw_w, 0, hw_h, 0, 1);
        glMatrixMode(GL_MODELVIEW);

        // redraw the screen entirely...
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT);

        const Vector< sp<LayerBase> >& layers(mVisibleLayersSortedByZ);
        const size_t count = layers.size();
        for (size_t i=0 ; i<count ; ++i) {
            const sp<LayerBase>& layer(layers[i]);
            layer->drawForSreenShot();
        }

        // XXX: this is needed on tegra
        glScissor(0, 0, sw, sh);

        // check for errors and return screen capture
        if (glGetError() != GL_NO_ERROR) {
            // error while rendering
            result = INVALID_OPERATION;
        } else {
            // allocate shared memory large enough to hold the
            // screen capture
            sp<MemoryHeapBase> base(
                    new MemoryHeapBase(size, 0, "screen-capture") );
            void* const ptr = base->getBase();
            if (ptr) {
                // capture the screen with glReadPixels()
                glReadPixels(0, 0, sw, sh, GL_RGBA, GL_UNSIGNED_BYTE, ptr);
                if (glGetError() == GL_NO_ERROR) {
                    *heap = base;
                    *w = sw;
                    *h = sh;
                    *f = PIXEL_FORMAT_RGBA_8888;
                    result = NO_ERROR;
                }
            } else {
                result = NO_MEMORY;
            }
        }
        glEnable(GL_SCISSOR_TEST);
        glViewport(0, 0, hw_w, hw_h);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);


    } else {
        result = BAD_VALUE;
    }

    // release FBO resources
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
    glDeleteRenderbuffersOES(1, &tname);
    glDeleteFramebuffersOES(1, &name);

    hw.compositionComplete();

    return result;
}


status_t SurfaceFlinger::captureScreen(DisplayID dpy,
        sp<IMemoryHeap>* heap,
        uint32_t* width, uint32_t* height, PixelFormat* format,
        uint32_t sw, uint32_t sh)
{
		   LOGI("into captureScreen");           
    // only one display supported for now
    if (UNLIKELY(uint32_t(dpy) >= DISPLAY_COUNT))
        return BAD_VALUE;

    if (!GLExtensions::getInstance().haveFramebufferObject())
        return INVALID_OPERATION;

    class MessageCaptureScreen : public MessageBase {
        SurfaceFlinger* flinger;
        DisplayID dpy;
        sp<IMemoryHeap>* heap;
        uint32_t* w;
        uint32_t* h;
        PixelFormat* f;
        uint32_t sw;
        uint32_t sh;
        status_t result;
    public:
        MessageCaptureScreen(SurfaceFlinger* flinger, DisplayID dpy,
                sp<IMemoryHeap>* heap, uint32_t* w, uint32_t* h, PixelFormat* f,
                uint32_t sw, uint32_t sh)
            : flinger(flinger), dpy(dpy),
              heap(heap), w(w), h(h), f(f), sw(sw), sh(sh), result(PERMISSION_DENIED)
        {

        }
        status_t getResult() const {
		   LOGI("getResult()");           
		 return result;
        }
        virtual bool handler() {


   LOGI("handler()");
            Mutex::Autolock _l(flinger->mStateLock);

            // if we have secure windows, never allow the screen capture
            if (flinger->mSecureFrameBuffer)
                return true;

            result = flinger->captureScreenImplLocked(dpy,
                    heap, w, h, f, sw, sh);

            return true;
        }
    };
   LOGI("before messagecapturescreen");
    sp<MessageBase> msg = new MessageCaptureScreen(this,
            dpy, heap, width, height, format, sw, sh);
    status_t res = postMessageSync(msg);
    if (res == NO_ERROR) {
        res = static_cast<MessageCaptureScreen*>( msg.get() )->getResult();
    }
    return res;
}