httpparser = {
	source = path.join(dependencies.basePath, "httpparser"),
}

function httpparser.import()
	httpparser.includes()
end

function httpparser.includes()
	includedirs {
		path.join(httpparser.source, "src"),
	}
end

function httpparser.project()

end

table.insert(dependencies, httpparser)
