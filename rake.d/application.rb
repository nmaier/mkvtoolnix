class Application < Target
  def initialize(exe)
    super exe + c(:EXEEXT)
  end

  def create_specific
    desc @desc if @aliases.empty? && !@desc.empty?
    file @target => @dependencies do |t|
      runq "    LINK #{t.name}", "#{c(:CXX)} #{c(:LDFLAGS)} #{c(:LIBDIRS)} #{$system_libdirs} -o #{t.name} #{@objects.join(" ")} #{@libraries.join(" ")}"
    end
    self
  end
end
