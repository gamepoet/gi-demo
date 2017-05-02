//
//  ViewController.swift
//  gi-demo
//
//  Created by Ben Scott on 4/28/17.
//  Copyright © 2017 Ben Scott. All rights reserved.
//

import Cocoa

class ViewController: NSViewController {
  override func viewDidLoad() {
    super.viewDidLoad()

    // Do any additional setup after loading the view.
  }

  override func keyDown(with theEvent: NSEvent) {
    switch (theEvent.keyCode) {
    case 0:
      app_input_key_down(APP_KEY_CODE_A);
      break;
    case 2:
      app_input_key_down(APP_KEY_CODE_D);
      break;
    case 14:
      app_input_key_down(APP_KEY_CODE_E);
      break;
    case 12:
      app_input_key_down(APP_KEY_CODE_Q);
      break;
    case 1:
      app_input_key_down(APP_KEY_CODE_S);
      break;
    case 13:
      app_input_key_down(APP_KEY_CODE_W);
      break;
    case 123:
      app_input_key_down(APP_KEY_CODE_LEFT);
      break;
    case 124:
      app_input_key_down(APP_KEY_CODE_RIGHT);
      break;
    case 125:
      app_input_key_down(APP_KEY_CODE_DOWN);
      break;
    case 126:
      app_input_key_down(APP_KEY_CODE_UP);
      break;
    default:
      super.keyDown(with: theEvent)
      break;
    }
  }

  override func keyUp(with theEvent: NSEvent) {
    switch (theEvent.keyCode) {
    case 0:
      app_input_key_up(APP_KEY_CODE_A);
      break;
    case 2:
      app_input_key_up(APP_KEY_CODE_D);
      break;
    case 14:
      app_input_key_up(APP_KEY_CODE_E);
      break;
    case 12:
      app_input_key_up(APP_KEY_CODE_Q);
      break;
    case 1:
      app_input_key_up(APP_KEY_CODE_S);
      break;
    case 13:
      app_input_key_up(APP_KEY_CODE_W);
      break;
    case 123:
      app_input_key_up(APP_KEY_CODE_LEFT);
      break;
    case 124:
      app_input_key_up(APP_KEY_CODE_RIGHT);
      break;
    case 125:
      app_input_key_up(APP_KEY_CODE_DOWN);
      break;
    case 126:
      app_input_key_up(APP_KEY_CODE_UP);
      break;
    default:
      super.keyUp(with: theEvent)
      break;
    }
  }

  override var representedObject: Any? {
    didSet {
    // Update the view, if already loaded.
    }
  }
}
