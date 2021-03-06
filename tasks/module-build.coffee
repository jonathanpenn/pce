fs = require('fs')
path = require('path')

moduleWrapperStart = """
module.exports = function(deps, opts) {

"""
moduleWrapperEnd = """

return Module;
}
"""

# wraps emscripten output for a single arch in a commonjs module

module.exports = (grunt, target, done) ->
  prelude = fs.readFileSync(path.resolve('commonjs/prelude.js'), encoding: 'utf8')
  epilogue = fs.readFileSync(path.resolve('commonjs/epilogue.js'), encoding: 'utf8')

  outputDirPath = path.join(path.resolve('commonjs/'), "pcejs-#{target}/lib/")
  try
    fs.mkdirSync(outputDirPath)
  catch err
    throw err unless err.code is 'EEXIST'
  
  outStream = fs.createWriteStream(path.join(outputDirPath, "pcejs-#{target}.js"), encoding: 'utf8')
  outStream.on 'close', ->
    grunt.log.ok("wrote commonjs/pcejs-#{target}/lib/pcejs-#{target}.js")
    done()

  outStream.write(moduleWrapperStart)
  outStream.write(prelude)
  
  grunt.log.ok("reading dist/pce-#{target}.js")

  fs.createReadStream(path.resolve("dist/pce-#{target}.js"), encoding: 'utf8')
    .on 'end', ->
      grunt.log.ok("end dist/pce-#{target}.js")
      outStream.write(epilogue)
      outStream.end(moduleWrapperEnd)
    .pipe(outStream, end: false)

