//
//  main.swift
//  testSortIndex
//
//  Created by Kenneth Durbrow on 4/24/17.
//  Copyright © 2017 Kenneth Durbrow. All rights reserved.
//

import Foundation

let indexFileName = CommandLine.arguments[1]

var index = try! Data(contentsOf: URL(fileURLWithPath: indexFileName), options: .alwaysMapped)

print(index.count)
