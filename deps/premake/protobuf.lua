protobuf = {
    includePath = path.join(dependencies.basePath, "protobuf/include"),  -- Path to protobuf includes
    libPath = path.join(dependencies.basePath, "protobuf/lib"),          -- Path to protobuf libraries
    binPath = path.join(dependencies.basePath, "protobuf/bin"),          -- Path to protobuf binaries (DLLs)
    protoSource = "./source/server/protobufs",                           -- Path to proto files
    protoOutput = "./build/src/protobufs/generated",  -- Correct output path
}

-- Import Protobuf into the project
function protobuf.import()
    protobuf.links()       -- Link necessary protobuf libraries
    protobuf.includes()    -- Include protobuf headers
    protobuf.copyDlls()    -- Copy protobuf DLLs (optional)
end

-- Set Protobuf include directories
function protobuf.includes()
    includedirs {
        protobuf.includePath,
        protobuf.protoOutput,  -- Include the output path for generated headers
    }
end

-- Link Protobuf libraries
function protobuf.links()
    libdirs { protobuf.libPath }  -- Set library path for protobuf

    filter "configurations:Debug"
        libdirs { path.join(protobuf.libPath, "debug") }
        links {
            "libprotobufd.lib",
            "libprotobuf-lited.lib",
            "libprotocd.lib",
        }

    filter "system:windows"
        links {
            "Winmm",
            "Ws2_32",
            "Imm32",
            "Dwmapi",
        }

    filter "system:linux"
        links {
            "pthread",
            "dl",
            "z",
        }

    filter {}
end

-- Copy Protobuf DLLs (optional, for runtime dependencies on Windows)
function protobuf.copyDlls()
    filter "system:windows"
        postbuildcommands {
            '{COPY} "%{protobuf.binPath}/*.dll" "%{cfg.targetdir}"'
        }
    filter {}
end

function protobuf.generate()
    local outputDir = path.getabsolute(protobuf.protoOutput)

    -- Ensure the output directory exists
    if not os.isdir(outputDir) then
        print("Creating output directory:", outputDir)
        os.mkdir(outputDir)
    end

    local protoFiles = os.matchfiles(path.join(protobuf.protoSource, "*.proto"))
    for _, protoFile in ipairs(protoFiles) do
        -- Define absolute paths for the command
        local protoCompiler = path.getabsolute(path.join(protobuf.binPath, "protoc"))
        local protoSourceDir = path.getabsolute(protobuf.protoSource)
        local protoFileAbsolute = path.getabsolute(protoFile)

        -- Construct the Protobuf command
        local command = string.format(
            '"%s" --cpp_out="%s" --proto_path="%s" "%s"',
            protoCompiler, outputDir, protoSourceDir, protoFileAbsolute
        )

        -- Batch file configuration
        local batchDir = path.getabsolute("build")
        os.mkdir(batchDir)
        local batchFile = path.join(batchDir, "run_protobuf.bat")

        -- Write the batch file
        local file = io.open(batchFile, "w")
        if file then
            file:write("@echo off\n")
            file:write("echo Running Protobuf command...\n")
            file:write(command .. "\n")
            file:write("echo Completed with exit code %errorlevel%.\n")
            file:write("exit /b %errorlevel%\n")
            file:close()
            print("Batch file created at: " .. batchFile)
        else
            error("Failed to create batch file at " .. batchFile)
        end

        -- Execute the batch file
        local handle = io.popen('cmd /c "' .. batchFile .. '"')
        local output = handle:read("*a")
        local result = handle:close()

        -- Debug output
        print("Batch file output:\n" .. output)

        -- Check result
        if not result then
            error("Protobuf generation failed for " .. protoFile .. "\nOutput:\n" .. output)
        end

        -- Add `std_include.hpp` to the generated `.cc` file
        local generatedFile = path.join(outputDir, path.getbasename(protoFile) .. ".pb.cc")
        if os.isfile(generatedFile) then
            local file = io.open(generatedFile, "r+")
            if file then
                local content = file:read("*a")
                file:seek("set", 0)
                file:write('#include "std_include.hpp"\n' .. content)
                file:close()
                print("Added std_include.hpp to " .. generatedFile)
            else
                print("Failed to modify " .. generatedFile)
            end
        else
            print("Generated file not found: " .. generatedFile)
        end

        print("Protobuf generation succeeded for " .. protoFile)
    end
end













-- Add Protobuf to dependencies
table.insert(dependencies, protobuf)
