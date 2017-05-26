//
//  ViewController.swift
//  gi-demo
//
//  Created by Ben Scott on 4/28/17.
//  Copyright © 2017 Ben Scott. All rights reserved.
//

import Cocoa
import Carbon

class ViewController: NSViewController {
  override func viewDidLoad() {
    super.viewDidLoad()

    // Do any additional setup after loading the view.
  }

  override func keyDown(with theEvent: NSEvent) {
    if let app_key_code = input_map[Int(theEvent.keyCode)] {
      app_input_key_down(app_key_code);
    }
    else {
      super.keyDown(with: theEvent)
    }
  }

  override func keyUp(with theEvent: NSEvent) {
    if let app_key_code = input_map[Int(theEvent.keyCode)] {
      app_input_key_up(app_key_code);
    }
    else {
      super.keyUp(with: theEvent)
    }
  }

  override func flagsChanged(with theEvent: NSEvent) {
    let vkey:Int = Int(theEvent.keyCode)
    var key:AppKeyCode? = input_map[vkey]
    var is_down:Bool = false

    switch (vkey) {
    case kVK_Control,
         kVK_RightControl:
      if (theEvent.modifierFlags.contains(.control)) {
        is_down = true
      }
      break

    case kVK_Option,
         kVK_RightOption:
      if (theEvent.modifierFlags.contains(.option)) {
        is_down = true
      }

    case kVK_Shift,
         kVK_RightShift:
      if (theEvent.modifierFlags.contains(.shift)) {
        is_down = true
      }

    default:
      key = nil
    }

    if key != nil {
      if is_down {
        app_input_key_down(key!)
      }
      else {
        app_input_key_up(key!)
      }
    }
    else {
      super.flagsChanged(with: theEvent)
    }
  }

  override var representedObject: Any? {
    didSet {
    // Update the view, if already loaded.
    }
  }

  var input_map = [
    kVK_ANSI_A: APP_KEY_CODE_A,
    kVK_ANSI_D: APP_KEY_CODE_D,
    kVK_ANSI_E: APP_KEY_CODE_E,
    kVK_ANSI_R: APP_KEY_CODE_R,
    kVK_ANSI_Q: APP_KEY_CODE_Q,
    kVK_ANSI_S: APP_KEY_CODE_S,
    kVK_ANSI_W: APP_KEY_CODE_W,
    kVK_F1: APP_KEY_CODE_F1,
    kVK_F2: APP_KEY_CODE_F2,
    kVK_F3: APP_KEY_CODE_F3,
    kVK_F4: APP_KEY_CODE_F4,
    kVK_F5: APP_KEY_CODE_F5,
    kVK_F6: APP_KEY_CODE_F6,
    kVK_F7: APP_KEY_CODE_F7,
    kVK_F8: APP_KEY_CODE_F8,
    kVK_F9: APP_KEY_CODE_F9,
    kVK_F10: APP_KEY_CODE_F10,
    kVK_F11: APP_KEY_CODE_F11,
    kVK_F12: APP_KEY_CODE_F12,
    kVK_LeftArrow: APP_KEY_CODE_LEFT,
    kVK_RightArrow: APP_KEY_CODE_RIGHT,
    kVK_UpArrow: APP_KEY_CODE_UP,
    kVK_DownArrow: APP_KEY_CODE_DOWN,
    kVK_Option: APP_KEY_CODE_LALT,
    kVK_RightOption: APP_KEY_CODE_RALT,
    kVK_Control: APP_KEY_CODE_LCONTROL,
    kVK_RightControl: APP_KEY_CODE_RCONTROL,
    kVK_Shift: APP_KEY_CODE_LSHIFT,
    kVK_RightShift: APP_KEY_CODE_RSHIFT,
  ]
}
