-- Define the new module for EVPP DLLs
evpp_dlls = {
    binPath = path.join(dependencies.basePath, "evpp_dlls"),  -- Path to EVPP DLLs
}

-- Function to import the EVPP DLLs setup
function evpp_dlls.import()
    evpp_dlls.copyDlls()  -- Call the function to copy the EVPP DLLs
end

-- Function to copy EVPP DLLs after build
function evpp_dlls.copyDlls()
    local sourceDir = path.getrelative(os.getcwd(), evpp_dlls.binPath)  -- Source folder for DLLs
    local batchFileName = "copy_evpp_dlls.bat"
    local batchDir = path.join(os.getcwd(), "build")
    local batchFilePath = path.join(batchDir, batchFileName)

    local batchFileCommands = {}
    table.insert(batchFileCommands, "@echo off")
    table.insert(batchFileCommands, "echo Copying EVPP DLLs...")

    -- Copy the main DLLs
    table.insert(batchFileCommands, 'set "SourceDir=%~dp0\\..\\' .. sourceDir .. '"')
    table.insert(batchFileCommands, 'set "TargetDir=%~1"')
    table.insert(batchFileCommands, 'echo SourceDir=%SourceDir%')
    table.insert(batchFileCommands, 'echo TargetDir=%TargetDir%')

    table.insert(batchFileCommands, 'for %%f in ("%SourceDir%\\*.dll") do (')
    table.insert(batchFileCommands, '    echo Copying "%%f" to "%TargetDir%"')
    table.insert(batchFileCommands, '    copy /Y "%%f" "%TargetDir%"')
    table.insert(batchFileCommands, ')')

    -- Create the batch file in the build folder
    os.mkdir(batchDir)
    local batchFile = io.open(batchFilePath, "w")
    if batchFile then
        for _, cmd in ipairs(batchFileCommands) do
            batchFile:write(cmd .. "\n")
        end
        batchFile:close()
        print("Copy Evpp Dlls Batch file created successfully.")
    else
        error("Error: Could not create batch file at " .. batchFileFullPath)
    end

    -- Post-build command to execute the batch file
    postbuildmessage "Executing copy_evpp_dlls.bat to copy EVPP DLLs..."
    postbuildcommands { 'call "' .. batchFilePath .. '" "%{cfg.targetdir}"' }
end

-- Add evpp_dlls to dependencies
table.insert(dependencies, evpp_dlls)
