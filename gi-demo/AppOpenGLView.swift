//
//  AppOpenGLView.swift
//  gi-demo
//
//  Created by Ben Scott on 4/28/17.
//  Copyright © 2017 Ben Scott. All rights reserved.
//

import Cocoa
import OpenGL.GL3

class AppOpenGLView: NSOpenGLView {
  required init?(coder: NSCoder) {
    lastTime = Float(CACurrentMediaTime())

    super.init(coder: coder)

    let attrs: [NSOpenGLPixelFormatAttribute] = [
      UInt32(NSOpenGLPFAAccelerated),
      UInt32(NSOpenGLPFAColorSize),
      UInt32(32),
      UInt32(NSOpenGLPFADoubleBuffer),
      UInt32(NSOpenGLPFAOpenGLProfile),
      UInt32(NSOpenGLProfileVersion3_2Core),
      UInt32(0),
    ]

    self.pixelFormat = NSOpenGLPixelFormat(attributes: attrs)
    self.openGLContext = NSOpenGLContext(format: pixelFormat!, share: nil)

    // enable vsync
    self.openGLContext?.setValues([1], for: NSOpenGLCPSwapInterval)
  }

  deinit {
    CVDisplayLinkStop(displayLink!)
  }

  // enable view to accept key events
  override var acceptsFirstResponder: Bool {
    return true;
  }

  override func prepareOpenGL() {
    super.prepareOpenGL()

    func displayLinkOutputCallback(
      displayLink: CVDisplayLink,
      _ now: UnsafePointer<CVTimeStamp>,
      _ outputTime: UnsafePointer<CVTimeStamp>,
      _ flagsIn: CVOptionFlags,
      _ flagsOut: UnsafeMutablePointer<CVOptionFlags>,
      _ displayLinkContext: UnsafeMutableRawPointer?
      ) -> CVReturn {
      unsafeBitCast(displayLinkContext, to: AppOpenGLView.self).render()
      return kCVReturnSuccess
    }

    lastTime = Float(CACurrentMediaTime())

    CVDisplayLinkCreateWithActiveCGDisplays(&displayLink)
    CVDisplayLinkSetOutputCallback(
      displayLink!,
      displayLinkOutputCallback,
      UnsafeMutableRawPointer(Unmanaged.passUnretained(self).toOpaque())
    )
    CVDisplayLinkStart(displayLink!)
  }

  override func draw(_ dirtyRect: NSRect) {
    super.draw(dirtyRect);
    render()
  }

  private func render() {
    let context = self.openGLContext!
    CGLLockContext(context.cglContextObj!)
    context.makeCurrentContext()

    let now = Float(CACurrentMediaTime())
    let dt = now - lastTime;
    lastTime = now;

    app_render(dt)

    CGLFlushDrawable(context.cglContextObj!)
    CGLUnlockContext(context.cglContextObj!)
  }

  private var lastTime: Float
  private var displayLink: CVDisplayLink?
}
