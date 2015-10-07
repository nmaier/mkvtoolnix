class Target
  begin
    include Rake::DSL
  rescue
  end

  def initialize(target)
    @target       = target
    @aliases      = []
    @objects      = []
    @libraries    = []
    @dependencies = []
    @file_deps    = []
    @only_if      = true
    @debug        = {}
    @desc         = nil
    @namespace    = nil
  end

  def debug(category)
    @debug[category] = !@debug[category]
    self
  end

  def debug_do
    yield self
    self
  end

  def description(description)
    @desc = description
    self
  end

  def only_if(condition)
    @only_if = condition
    self
  end

  def end_if
    only_if true
  end

  def extract_options(list)
    options = list.empty? || !list.last.is_a?(Hash) ? {} : list.pop
    return list, options
  end

  def aliases(*list)
    @aliases += list.compact
    self
  end

  def sources(*list)
    list, options = extract_options list

    if @debug[:sources]
      puts "Target::sources: only_if #{@only_if}; list & options:"
      pp list
      pp options
    end

    return self if !@only_if || (options.include?(:if) && !options[:if])

    ext_map = {
      '.ui'  => 'h',
      '.moc' => '.moco',
    }

    obj_re          = /\.(?:moc)?o$/
    no_file_deps_re = /\.(?:moc)?o$/

    list           = list.collect { |e| e.respond_to?(:to_a) ? e.to_a : e }.flatten
    file_mode      = (options[:type] || :file) == :file
    except         = !file_mode && options[:except].is_a?(Array) ? options[:except].collect { |file| list.collect { |dir| "#{dir}/#{file}" } }.flatten.to_hash_by : {}
    new_sources    = list.collect { |entry| file_mode ? (entry.respond_to?(:to_a) ? entry.to_a : entry) : FileList["#{entry}/*.c", "#{entry}/*.cpp", "#{entry}/*.cc"].to_a }.flatten.select { |file| !except[file] }
    new_deps       = new_sources.collect { |file| [ file.ext(ext_map[ file.pathmap('%x') ] || 'o'), file ] }
    new_file_deps  = new_deps.reject { |src, tgt| no_file_deps_re.match src }
    @objects       = ( @objects      + new_deps.collect { |a| a.first }.select { |file| obj_re.match file } ).uniq
    @dependencies  = ( @dependencies + new_deps.collect { |a| a.first }                                     ).uniq
    @file_deps     = ( @file_deps    + new_file_deps                                                        ).uniq
    self
  end

  def dependencies(*list)
    @dependencies += list.flatten.select { |entry| !entry.blank? } if @only_if
    self
  end

  def png_icon(icon, *new_deps)
    icon_h     = FileList[icon].to_a.first.ext('h')
    new_deps   = @file_deps.select { |pair| /\.cpp$/.match(pair.last) }.collect(&:first) if new_deps.empty?
    @file_deps = new_deps.collect { |file| [ FileList[file].to_a.first.ext('o'), icon_h ] } + @file_deps
    self
  end

  def libraries(*list)
    list, options = extract_options list

    return self if !@only_if || (options.include?(:if) && !options[:if])

    list.flatten!

    @dependencies += list.collect do |entry|
      case entry
      when :mtxcommon   then "src/common/libmtxcommon.a"
      when :mtxinput    then "src/input/libmtxinput.a"
      when :mtxoutput   then "src/output/libmtxoutput.a"
      when :mtxmerge    then "src/merge/libmtxmerge.a"
      when :mtxinfo     then "src/info/libmtxinfo.a"
      when :mtxextract  then "src/extract/libmtxextract.a"
      when :mtxpropedit then "src/propedit/libmtxpropedit.a"
      when :mtxunittest then "tests/unit/libmtxunittest.a"
      when :avi         then "lib/avilib-0.6.10/libavi.a"
      when :rmff        then "lib/librmff/librmff.a"
      when :pugixml     then "lib/pugixml/src/libpugixml.a"
      when :mpegparser  then "src/mpegparser/libmpegparser.a"
      when :ebml        then c?("EBML_MATROSKA_INTERNAL") ? "lib/libebml/src/libebml.a"         : nil
      when :matroska    then c?("EBML_MATROSKA_INTERNAL") ? "lib/libmatroska/src/libmatroska.a" : nil
      when :gtest       then $gtest_internal              ? "lib/gtest/src/libgtest.a"          : nil
      else                   nil
      end
    end.compact

    @libraries += list.collect do |entry|
      case entry
      when nil               then nil
      when :curl             then c(:CURL_LIBS)
      when :magic            then c(:MAGIC_LIBS)
      when :flac             then c(:FLAC_LIBS)
      when :iconv            then c(:ICONV_LIBS)
      when :intl             then c(:LIBINTL_LIBS)
      when :boost_regex      then c(:BOOST_REGEX_LIB)
      when :boost_filesystem then c(:BOOST_FILESYSTEM_LIB)
      when :boost_system     then c(:BOOST_SYSTEM_LIB)
      when :qt               then c(:QT_LIBS)
      when :wxwidgets        then c(:WXWIDGETS_LIBS)
      when :mpegparser       then [ '-Lsrc/mpegparser', '-lmpegparser'  ]
      when :mtxinput         then [ '-Lsrc/input',      '-lmtxinput'    ]
      when :mtxoutput        then [ '-Lsrc/output',     '-lmtxoutput'   ]
      when :mtxmerge         then [ '-Lsrc/merge',      '-lmtxmerge'    ]
      when :mtxinfo          then [ '-Lsrc/info',       '-lmtxinfo'     ]
      when :mtxextract       then [ '-Lsrc/extract',    '-lmtxextract'  ]
      when :mtxpropedit      then [ '-Lsrc/propedit',   '-lmtxpropedit' ]
      when :mtxunittest      then [ '-Ltests/unit',     '-lmtxunittest' ]
      when String            then entry
      else                        "-l#{entry}"
      end
    end.compact

    self
  end

  def dump
    %w{aliases sources objects dependencies libraries}.each do |type|
      puts "@#{type}:"
      pp instance_variable_get("@#{type}")
    end
    self
  end

  def create
    definition = Proc.new do
      @file_deps.each do |spec|
        file spec.first => spec.last unless spec.first == spec.last
      end

      @aliases.each_with_index do |name, idx|
        desc @desc if (0 == idx) && !@desc.empty?
        task name => @target
      end
    end

    if @namespace
      namespace @namespace, &definition
    else
      definition.call
    end

    create_specific

    self
  end
end
