class Application < Target
  def initialize(exe)
    super exe + c(:EXEEXT)
    @namespace = :apps
  end

  def create_specific
    libraries :rpcrt4, :if => c?(:MINGW)

    namespace :apps do
      desc @desc if @aliases.empty? && !@desc.empty?
      file @target => @dependencies do |t|
        runq "    LINK #{t.name}", "#{c(:CXX)} #{$flags[:ldflags]} #{$system_libdirs} -o #{t.name} #{@objects.join(" ")} #{@libraries.join(" ")}"
      end
    end
    self
  end
end
