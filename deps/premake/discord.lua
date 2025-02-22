discord = {
    source = path.join(dependencies.basePath, "discord_sdk"),
}

function discord.includes()
    includedirs {
        path.join(discord.source, "cpp"),
        path.join(discord.source, "c")
    }
end

function discord.import()
    discord.includes()
    
    filter "platforms:x64"
        libdirs {
            path.join(discord.source, "lib/x86_64")
        }
        links {
            "discord_game_sdk.dll.lib"
        }
        
        files {
            path.join(discord.source, "cpp/**.cpp"),
            path.join(discord.source, "cpp/**.h")
        }
        
        -- Exclude these files from PCH
        filter { "files:" .. path.join(discord.source, "cpp/**.cpp") }
            flags { "NoPCH" }
        filter {}
    filter {}
end

function discord.project()
    -- No need for a static lib project, we'll use the SDK directly
end

table.insert(dependencies, discord)
