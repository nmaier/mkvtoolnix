// l1 -------------------------------------------------------

      if (!in_parent(l0)) {
        delete l1;
        break;
      }

      if (upper_lvl_el > 0) {
        upper_lvl_el--;
        if (upper_lvl_el > 0)
          break;
        delete l1;
        l1 = l2;
        continue;

      } else if (upper_lvl_el < 0) {
        upper_lvl_el++;
        if (upper_lvl_el < 0)
          break;

      }

      l1->SkipData(*es, l1->Generic().Context);
      delete l1;
      l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                               0xFFFFFFFFL, true);

// l2 -------------------------------------------------------

          if (!in_parent(l1)) {
            delete l2;
            break;
          }

          if (upper_lvl_el > 0) {
            upper_lvl_el--;
            if (upper_lvl_el > 0)
              break;
            delete l2;
            l2 = l3;
            continue;

          } else if (upper_lvl_el < 0) {
            upper_lvl_el++;
            if (upper_lvl_el < 0)
              break;

          }

          l2->SkipData(*es, l2->Generic().Context);
          delete l2;
          l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                   0xFFFFFFFFL, true);

// l3 -------------------------------------------------------

              if (!in_parent(l2)) {
                delete l3;
                break;
              }

              if (upper_lvl_el > 0) {
                upper_lvl_el--;
                if (upper_lvl_el > 0)
                  break;
                delete l3;
                l3 = l4;
                continue;

              } else if (upper_lvl_el < 0) {
                upper_lvl_el++;
                if (upper_lvl_el < 0)
                  break;

              }

              l3->SkipData(*es, l3->Generic().Context);
              delete l3;
              l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                       0xFFFFFFFFL, true);

  /* -------------------------------------------------------- */

              if (!in_parent(l2)) {
                delete l3;
                break;
              }

              if (upper_lvl_el < 0) {
                upper_lvl_el++;
                if (upper_lvl_el < 0)
                  break;

              }

              l3->SkipData(*es, l3->Generic().Context);
              delete l3;
              l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                       0xFFFFFFFFL, true);

// l4 -------------------------------------------------------

                  if (!in_parent(l3)) {
                    delete l4;
                    break;
                  }

                  if (upper_lvl_el > 0) {
                    upper_lvl_el--;
                    if (upper_lvl_el > 0)
                      break;
                    delete l4;
                    l4 = l5;
                    continue;

                  } else if (upper_lvl_el < 0) {
                    upper_lvl_el++;
                    if (upper_lvl_el < 0)
                      break;

                  }

                  l4->SkipData(*es, l4->Generic().Context);
                  delete l4;
                  l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                           0xFFFFFFFFL, true);

  /* -------------------------------------------------------- */

                  if (!in_parent(l3)) {
                    delete l4;
                    break;
                  }

                  if (upper_lvl_el < 0) {
                    upper_lvl_el++;
                    if (upper_lvl_el < 0)
                      break;

                  }

                  l4->SkipData(*es, l4->Generic().Context);
                  delete l4;
                  l4 = es->FindNextElement(l3->Generic().Context,
                                           upper_lvl_el, 0xFFFFFFFFL, true);

// l5 -------------------------------------------------------

                      if (!in_parent(l4)) {
                        delete l5;
                        break;
                      }

                      if (upper_lvl_el > 0) {
                        upper_lvl_el--;
                        if (upper_lvl_el > 0)
                          break;
                        delete l5;
                        l5 = l6;
                        continue;

                      } else if (upper_lvl_el < 0) {
                        upper_lvl_el++;
                        if (upper_lvl_el < 0)
                          break;

                      }

                      l5->SkipData(*es, l5->Generic().Context);
                      delete l5;
                      l5 = es->FindNextElement(l4->Generic().Context,
                                               upper_lvl_el, 0xFFFFFFFFL,
                                               true);

  /* -------------------------------------------------------- */

                      if (!in_parent(l4)) {
                        delete l5;
                        break;
                      }

                      if (upper_lvl_el < 0) {
                        upper_lvl_el++;
                        if (upper_lvl_el < 0)
                          break;

                      }

                      l5->SkipData(*es, l5->Generic().Context);
                      delete l5;
                      l5 = es->FindNextElement(l4->Generic().Context,
                                               upper_lvl_el, 0xFFFFFFFFL,
                                               true);

// l6 -------------------------------------------------------

                          if (!in_parent(l5)) {
                            delete l6;
                            break;
                          }

                          if (upper_lvl_el > 0) {
                            upper_lvl_el--;
                            if (upper_lvl_el > 0)
                              break;
                            delete l6;
                            l6 = l7;
                            continue;

                          } else if (upper_lvl_el < 0) {
                            upper_lvl_el++;
                            if (upper_lvl_el < 0)
                              break;

                          }

                          l6->SkipData(*es, l6->Generic().Context);
                          delete l6;
                          l6 = es->FindNextElement(l5->Generic().Context,
                                                   upper_lvl_el,
                                                   0xFFFFFFFFL, true);
