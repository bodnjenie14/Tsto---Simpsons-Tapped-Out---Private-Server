-- Define the steamworks module
steamworks = {
    includePaths = {
        path.join(dependencies.basePath, "steamworks_sdk"),
        path.join(dependencies.basePath, "steamworks_sdk/public/steam"),
    },
    libPaths = {
        path.join(dependencies.basePath, "steamworks_sdk/redistributable_bin/win64"),
        path.join(dependencies.basePath, "steamworks_sdk/public/steam/lib/win64"),
    },
    libs = {
        "steam_api64.lib",
        "sdkencryptedappticket64.lib",
    },
    dlls = {
        path.join(dependencies.basePath, "steamworks_sdk/redistributable_bin/win64/steam_api64.dll"),
        path.join(dependencies.basePath, "steamworks_sdk/public/steam/lib/win64/sdkencryptedappticket64.dll"),
    },
}

--func to import
function steamworks.import()
    steamworks.includes()    
    steamworks.links()       
    steamworks.copyDlls()    
end

-- Function to set Steamworks dirs
function steamworks.includes()
    includedirs(steamworks.includePaths)
end

-- function to link Steam
function steamworks.links()
    libdirs(steamworks.libPaths)
    links(steamworks.libs)
end

-- Func to copy Steamworks DLLs after build
function steamworks.copyDlls()
    
    local batchFileName = "copy_steamworks_dlls.bat"
    local batchDir = path.join(os.getcwd(), "build")
    local batchFilePath = path.join(batchDir, batchFileName)

    local batchFileCommands = {}
    table.insert(batchFileCommands, "@echo off")
    table.insert(batchFileCommands, "echo Copying Steamworks DLLs...")
	
	-- Copy main DLLs

    table.insert(batchFileCommands, 'set "TargetDir=%~1"')
    table.insert(batchFileCommands, 'echo TargetDir=%TargetDir%')

    for _, dllPath in ipairs(steamworks.dlls) do
        local relativeDllPath = path.getrelative(batchDir, dllPath)
        relativeDllPath = relativeDllPath:gsub("/", "\\")
		
        table.insert(batchFileCommands, 'set "SourceDll=%~dp0\\..\\' .. relativeDllPath .. '"')
        table.insert(batchFileCommands, 'echo Copying "%SourceDll%" to "%TargetDir%"')
        table.insert(batchFileCommands, 'copy /Y "%SourceDll%" "%TargetDir%"')
    end

    -- Create the bat file on premake to build folder
    do
        -- Create dir
        os.mkdir(batchDir)

        local batchFileFullPath = batchFilePath
        print("Creating Steamworks batch file at: " .. batchFileFullPath)

        -- Open the bat file for writing
        local batchFile = io.open(batchFileFullPath, "w")
        if batchFile then
            for _, cmd in ipairs(batchFileCommands) do
                batchFile:write(cmd .. "\n")
            end
            batchFile:close()
            print("Steamworks batch file created successfully.")
        else
            error("Error: Could not create batch file at " .. batchFileFullPath)
        end
    end

    -- Print msg for post-build step
    postbuildmessage "Executing copy_steamworks_dlls.bat to copy Steamworks DLLs..."

    -- Execute the bat file from the build directory
    postbuildcommands { 'call "' .. batchFilePath .. '" "%{cfg.targetdir}"' }
end

table.insert(dependencies, steamworks)
