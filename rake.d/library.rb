class Library < Target
  def initialize(name)
    super name
    @build_dll = false
  end

  def build_dll(build_dll_as_well = true)
    @build_dll = build_dll_as_well
    self
  end

  def create_specific
    file "#{@target}.a" => @objects do |t|
      FileUtils.rm_f t.name
      runq "      AR #{t.name}", "#{c(:AR)} rcu #{t.name} #{@objects.join(" ")}"
      runq "  RANLIB #{t.name}", "#{c(:RANLIB)} #{t.name}"
    end

    return self unless @build_dll

    file "#{@target}.dll" => @objects do |t|
      runq "  LD/DLL #{t.name}", <<-COMMAND
        #{c(:CXX)} #{$flags[:ldflags]} #{$system_libdirs} -shared -Wl,--export-all -Wl,--out-implib=#{t.name}.a -o #{t.name} #{@objects.join(" ")} #{@libraries.join(" ")}
      COMMAND
    end

    self
  end
end
