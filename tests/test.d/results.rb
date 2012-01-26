class Results
  attr_reader :results

  def initialize(controller)
    @controller = controller

    load
  end

  def load
    @results = Hash.new
    return unless FileTest.exist?("results.txt")

    IO.readlines("results.txt").each do |line|
      parts    =  line.chomp.split(/:/)
      parts[3] =~ /([0-9]{4})([0-9]{2})([0-9]{2})-([0-9]{2})([0-9]{2})([0-9]{2})/
      parts    << 0 if 4 <= parts.size

      @results[parts[0]] = {
        :hash            => parts[1],
        :status          => parts[2].to_sym,
        :date_added      => Time.local($1, $2, $3, $4, $5, $6),
        :duration        => parts[4].to_f,
      }
    end
  end

  def save
    f = File.new "results.txt", "w"
    @results.keys.sort.each do |key|
      f.puts [ key, @results[key][:hash], @results[key][:status], @results[key][:date_added].strftime("%Y%m%d-%H%M%S"), @results[key][:duration] ].collect { |item| item.to_s }.join(":")
    end
    f.close
  end

  def exist?(name)
    @results.has_key? name
  end

  def test_attr(name, attribute, default = nil)
    if !exist? name
      return default if default
      raise "No such result #{name}"
    end
    @results[name][attribute]
  end

  %w{hash status date_added duration}.each do |attribute|
    define_method("#{attribute}?") do |name|
      test_attr name, attribute.to_sym, attribute == "duration" ? 0 : nil
    end
  end

  def add(name, hash, duration = 0)
    raise "Test does already exist" if exist? name
    @results[name] = {
      :hash        => hash,
      :status      => :new,
      :date_added  => Time.now,
      :duration    => duration,
    }
    save
  end

  def set(name, status)
    return unless exist? name
    @results[name][:status] = status
    save
  end

  def set_hash(name, hash)
    raise "Test does not exist" unless exist? name
    @results[name][:hash] = hash
    save
  end

  def set_duration(name, duration)
    return unless exist?(name) && @controller.record_duration
    @results[name][:duration] = duration
    save
  end
end
