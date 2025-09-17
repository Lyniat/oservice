$test_data = {
        "this_is_data?" => true,
        :letters => [
            "a",
            "b",
            "c",
            "d",
        ],
        "numbers" => [
            [1, 1.1],
            [2, 2.2],
            [-3, -3.3],
        ],
        "random_string_in_between": "--",
        "dev_info" => {
            "gender" => "raccoon",
            :age => 256,
            "name" => "lyniat",
            "knows_how_to_code" => true,
            "knows_how_to_code_good" => false,
            :credit_card => {
                "type" => "Coders Club",
                "card number" => "0016 0032 0064 0128",
            },
        },
        "what_causes_exceptions?": nil,
        "different_keys": {
            567 => "567",
            "abc" => "ABC",
            3.14 => "PI",
        },
        "end_of_data" => true,
    }

def boot args
  $gtk.dlopen 'oservice'
  OService.init_api
end

def oservice_update(type, update_value)
    puts type
    puts update_value
end

def tick args
    if args.state.tick_count == 0
        args.state.send_always = true
        # test for auto join and set value
        tj = OService.get_lobby_to_join
      if tj != nil
        args.state.lobby_to_join = tj
        OService.list_lobbies
      end
    end
    if args.state.send_always
        OService.send_to_host($test_data,0,1)
    end
end