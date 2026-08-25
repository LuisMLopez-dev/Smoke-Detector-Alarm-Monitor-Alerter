## Overview
The files contained in this subfolder, Subsystem Testing, are several microcontroller test sketches of the subsystems used in the project/product.

The listener unit and the alarm units' sketches were broken down into their core subsystems, and each subsystem was then made into its own test sketch to allow for easier validation of the subsystem behavior.

Each sub-folder and file is named according to the subsystem that is being tested.

### Notes on Comments
Each test sketch has a structured comment block at the top that consists of:
- Test Name
- Purpose
- Method
- Expected Results
- Notes (If any)

These comments serve to provide a high-level understanding of the sketch and what the test is validating and the expected behavior.

Very few inline comments are used within the code itself because these sketches are intended for functional testing and not for a detailed code explanation. The sketches for the full system have much more comprehensive, line-by-line documentation and explanations for those who want to understand the implementation of the code. 

The sketches are to isolate and validate the subsystem functionality and behavior, not to break down the system architecture. 
