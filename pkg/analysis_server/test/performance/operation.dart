// Copyright (c) 2015, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

library server.operation;

import 'dart:async';

import 'package:analysis_server/src/protocol.dart';
import 'package:logging/logging.dart';

import 'driver.dart';
import 'input_converter.dart';

/**
 * An [Operation] represents an action such as sending a request to the server.
 */
abstract class Operation {
  Future perform(Driver driver);
}

/**
 * A [RequestOperation] sends a [JSON] request to the server.
 */
class RequestOperation extends Operation {
  final CommonInputConverter converter;
  final Map<String, dynamic> json;

  RequestOperation(this.converter, this.json);

  @override
  Future perform(Driver driver) {
    Stopwatch stopwatch = new Stopwatch();
    String method = json['method'];
    driver.logger.log(Level.FINE, 'Sending request: $method\n  $json');
    stopwatch.start();
    void recordResponse(bool success, response) {
      stopwatch.stop();
      Duration elapsed = stopwatch.elapsed;
      driver.results.record(method, elapsed, success: success);
      driver.logger.log(
          Level.FINE, 'Response received: $method : $elapsed\n  $response');
    }
    driver.send(method, json['params']).then((response) {
      recordResponse(true, response);
    }).catchError((e, s) {
      recordResponse(false, e);
      converter.recordErrorResponse(json, e);
    });
    return null;
  }
}

class StartServerOperation extends Operation {
  @override
  Future perform(Driver driver) {
    return driver.startServer();
  }
}

class WaitForAnalysisCompleteOperation extends Operation {
  @override
  Future perform(Driver driver) {
    DateTime start = new DateTime.now();
    driver.logger.log(Level.FINE, 'waiting for analysis to complete');
    StreamSubscription<ServerStatusParams> subscription;
    Timer timer;
    Completer completer = new Completer();
    bool isAnalyzing = false;
    subscription = driver.onServerStatus.listen((ServerStatusParams params) {
      // TODO (danrubel) ensure that server.setSubscriptions STATUS is set
      if (params.analysis != null) {
        if (params.analysis.isAnalyzing) {
          isAnalyzing = true;
        } else {
          subscription.cancel();
          timer.cancel();
          DateTime end = new DateTime.now();
          Duration delta = end.difference(start);
          driver.logger.log(Level.FINE, 'analysis complete after $delta');
          completer.complete();
          driver.results.record('analysis complete', delta);
        }
      }
    });
    timer = new Timer.periodic(new Duration(milliseconds: 20), (_) {
      if (!isAnalyzing) {
        // TODO (danrubel) revisit this once source change requests are implemented
        subscription.cancel();
        timer.cancel();
        driver.logger.log(Level.INFO, 'analysis never started');
        completer.complete();
        return;
      }
      // Timeout if no communcation received within the last 10 seconds.
      double currentTime = driver.server.currentElapseTime;
      double lastTime = driver.server.lastCommunicationTime;
      if (currentTime - lastTime > 10) {
        subscription.cancel();
        timer.cancel();
        String message = 'gave up waiting for analysis to complete';
        driver.logger.log(Level.WARNING, message);
        completer.completeError(message);
      }
    });
    return completer.future;
  }
}
